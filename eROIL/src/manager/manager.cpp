#include "manager.h"
#include <cstring>
#include <thread>
#include <chrono>
#include <memory>
#include <algorithm>
#include <eROIL/print.h>
#include "types/types.h"
#include "timer/timer.h"
#include "platform/platform.h"

namespace eroil {
    static int32_t unique_id() {
        static std::atomic<int> next_id{0};
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    Manager::Manager(ManagerConfig cfg) 
        : m_id(cfg.id), 
        m_router{}, 
        m_context{},
        m_comms{cfg.id, m_router},
        m_broadcast{},
        m_valid(false) {

        // confirm we know who the fuck we are
        if (static_cast<size_t>(m_id) > cfg.nodes.size()) {
            ERR_PRINT("manager has no entry in nodes info list, manager cannot initialize");
            m_valid = false;
            return;
        }
        
        m_valid = addr::insert_addresses(cfg.nodes[m_id], cfg.nodes, cfg.mode);
    }

    bool Manager::init() {
        if (!m_valid) {
            ERR_PRINT("manager cannot initialize! figure out wtf is wrong");
            return false;
        }

        m_comms.start();
        start_broadcast();
        return true;
    }

    SendHandle* Manager::open_send(OpenSendData data) {
        auto handle = std::make_unique<SendHandle>(unique_id(), data);
        SendHandle* raw_handle = handle.get();
        m_router.register_send_publisher(std::move(handle));
        return raw_handle;
    }

    void Manager::send_label(SendHandle* handle, void* buf, size_t buf_size, size_t buf_offset) {
        // figure out which buffer to use
        uint8_t* send_buf = nullptr;
        size_t send_size = 0;
        size_t send_offset = 0;
        if (buf != nullptr) {
            send_buf = reinterpret_cast<uint8_t*>(buf);
            send_size = buf_size;
            send_offset = buf_offset;
        } else {
            send_buf = handle->data->buf;
            send_size = handle->data->buf_size;
            send_offset = handle->data->buf_offset;
        }

        // attach header for send
        uint16_t flags = 0;
        set_flag(flags, LabelFlag::Data);
        LabelHeader hdr {
            MAGIC_NUM,
            VERSION,
            m_id,
            flags,
            handle->data->label,
            static_cast<uint32_t>(send_size)
        };
        
        size_t data_size = send_size + sizeof(hdr);
        auto data = std::make_unique<uint8_t[]>(data_size);
        std::memcpy(data.get(), &hdr, sizeof(hdr));
        std::memcpy(data.get() + sizeof(hdr), send_buf + send_offset, send_size);
        
        m_comms.send_label(handle->uid, handle->data->label, data_size, std::move(data));
    }

    void Manager::close_send(SendHandle* handle) {
        m_router.unregister_send_publisher(handle);
    }

    RecvHandle* Manager::open_recv(OpenReceiveData data) {
        auto handle = std::make_unique<RecvHandle>(unique_id(), data);
        RecvHandle* raw_handle = handle.get();
        m_router.register_recv_subscriber(std::move(handle));
        return raw_handle;
    }

    void Manager::close_recv(RecvHandle* handle) {
        m_router.unregister_recv_subscriber(handle);
    }

    bool Manager::start_broadcast() {
        sock::UdpMcastConfig cfg;
        auto result = m_broadcast.open_and_join(cfg);
        if (result.code != sock::SockErr::None) {
            ERR_PRINT("udp multicast group join failed, broadcast exiting");
            print_socket_result(result);
            return false;
        }

        std::thread send_broadcast_thread([this]() {
            while (true) {
                send_broadcast();
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        });
        send_broadcast_thread.detach();

        std::thread recv_broadcast_thread([this]() {
            plat::affinitize_current_thread(2);
            while (true) { recv_broadcast(); }
        });
        recv_broadcast_thread.detach();
        
        return true;
    }

    void Manager::send_broadcast() {
        BroadcastMessage msg;
        msg.id = m_id;
        msg.send_labels = m_router.get_send_labels();
        msg.recv_labels = m_router.get_recv_labels();
      
        m_broadcast.send_broadcast(&msg, sizeof(msg));
    }

    void Manager::recv_broadcast() {
        BroadcastMessage msg;
        m_broadcast.recv_broadcast(&msg, sizeof(msg));
        if (msg.id == m_id) return;
        
        // time how long this takes
        //time::Timer t("recv_broadcast()");

        // filter out invalid labels
        std::vector<LabelInfo> recv_labels;
        recv_labels.reserve(msg.recv_labels.size());
        for (const LabelInfo& l : msg.recv_labels) {
            if (l.label > INVALID_LABEL) recv_labels.push_back(l);
        }

        // check if they want a label we send
        add_new_subscribers(msg.id, recv_labels);

        // check if they removed their subscription from our labels
        remove_subscription(msg.id, recv_labels);

        // filter out invalid labels
        std::vector<LabelInfo> send_labels;
        send_labels.reserve(msg.send_labels.size());
        for (const LabelInfo& l : msg.send_labels) {
            if (l.label > INVALID_LABEL) send_labels.push_back(l);
        }

        // check if we want a label they send
        add_new_publisher(msg.id, send_labels);

        // check if they are no longer the publisher of a label we want
        remove_publisher(msg.id, send_labels);
    }

    void Manager::add_new_subscribers(const NodeId source_id, const std::vector<LabelInfo>& recv_labels) {
        for (const auto& info : recv_labels) {
            // if we do not send this label, ignore it
            if (!m_router.has_send_route(info.label)) continue;

            // check if they're already on subscriber list
            if (m_router.is_send_subscriber(info.label, source_id)) continue;

            // this is a label they want to recv, but are not on the subscription list
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Shm: {
                    PRINT("adding local send subscriber, nodeid=", source_id, " label=", info.label);
                    m_router.add_local_send_subscriber(info.label, info.size, m_id, source_id);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("adding remote send subscriber, nodeid=", source_id, " label=", info.label);
                    m_router.add_remote_send_subscriber(info.label, info.size, source_id);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add send subscriber but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }

            // check if they no longer want a label we send them
            // if they are ensure that label appears in their list still
        }
    }

    void Manager::remove_subscription(const NodeId source_id, const std::vector<LabelInfo>& recv_labels) {
        // all the labels we send
        auto send_labels = m_router.get_send_labels();
        for (const auto& [label, _] : send_labels) {
            // if theyre not a recver of this label, ignore it
            if (!m_router.is_send_subscriber(label, source_id)) continue;

            // they're a subscriber, do they still subscriber to this label?
            auto it = std::find_if(
                recv_labels.begin(),
                recv_labels.end(),
                [&](const LabelInfo& info) {
                    return info.label == label;
                }
            );
            if (it != recv_labels.end()) continue;

            // they no longer subscriber to this label, remove them
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Shm: {
                    PRINT("removing local send subscriber, nodeid=", source_id, " label=", label);
                    m_router.remove_local_send_subscriber(label, source_id);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("removing remote send subscriber, nodeid=", source_id, " label=", label);
                    m_router.remove_remote_send_subscriber(label, source_id);
                    break;
                }
                default: {
                    ERR_PRINT("tried to remove send subscriber but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }
        }
    }

    void Manager::add_new_publisher(const NodeId source_id, const std::vector<LabelInfo>& send_labels) {
        for (const auto& info : send_labels) {
            // if we do not want this label, ignore it
            if (!m_router.has_recv_route(info.label)) continue;

            // check if we're already subscribed
            if (m_router.is_recv_publisher(info.label, source_id)) continue;

            // this is a label we want to recv and have not registered a publisher
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Shm: {
                    // add local recv pub and start recv worker to watch shared memory block
                    PRINT("adding local recv publisher, nodeid=", source_id, " label=", info.label);
                    m_router.add_local_recv_publisher(info.label, info.size, m_id, source_id);
                    m_comms.start_local_recv_worker(info.label, info.size);
                    break;
                }
                case addr::RouteKind::Socket: {
                    // add remote recv pub, recv worker should already be listening to this socket
                    PRINT("adding remote recv publisher, nodeid=", source_id, " label=", info.label);
                    m_router.add_remote_recv_publisher(info.label, info.size, source_id);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add recv publisher but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }
        }
    }

    void Manager::remove_publisher(const NodeId source_id, const std::vector<LabelInfo>& send_labels) {
        // all the labels we recv
        auto recv_labels = m_router.get_recv_labels();
        for (const auto& [label, _] : recv_labels) {
            // are they a publisher of this label, if not continue
            if (!m_router.is_recv_publisher(label, source_id)) continue;

            // they are a publisher, do they still publish this label?
            auto it = std::find_if(
                send_labels.begin(),
                send_labels.end(),
                [&](const LabelInfo& info) {
                    return info.label == label;
                }
            );
            if (it != send_labels.end()) continue;

            // they no longer publish this label, remove them as a publisher
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Shm: {
                    PRINT("removing local recv publisher, nodeid=", source_id, " label=", label);
                    m_router.remove_local_recv_publisher(label, source_id);
                    m_comms.stop_local_recv_worker(label);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("removing remote recv publisher, nodeid=", source_id, " label=", label);
                    m_router.remove_remote_recv_publisher(label,source_id);
                    break;
                }
                default: {
                    ERR_PRINT("tried to remove recv publisher but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }
        }
    }
}