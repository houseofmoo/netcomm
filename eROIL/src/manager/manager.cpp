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
#include "log/evtlog_api.h"

namespace eroil {
    static int32_t unique_id() {
        static std::atomic<int> next_id{0};
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    Manager::Manager(cfg::ManagerConfig cfg) 
        : m_id(cfg.id), 
        m_cfg(cfg),
        m_router{}, 
        m_context{},
        m_comms{cfg.id, m_router},
        m_broadcast{},
        m_valid(false) {
                
        // confirm we know who the fuck we are
        if (static_cast<size_t>(m_id) > m_cfg.nodes.size()) {
            ERR_PRINT("manager has no entry in nodes info list, manager cannot initialize");
            m_valid = false;
            return;
        }

        m_valid = addr::insert_addresses(m_cfg.nodes[m_id], m_cfg.nodes, m_cfg.mode);
    }

    bool Manager::init() {
        if (!m_valid) {
            ERR_PRINT("manager cannot initialize! figure out wtf is wrong");
            return false;
        }

        // estimate tsc frequency (100 ms sleep)
        std::thread([]{
            plat::affinitize_current_thread_to_current_cpu();
            uint64_t tsc_hz = evtlog::estimate_tsc_hz();
            LOG("tsc frequency estimated: ", tsc_hz, " Hz");
        }).join();

        m_comms.start();
        start_broadcast();

        // wait about 1 minute then dump time info
        //time::time_log.timed_run(m_id, 60 * 1000);

        return true;
    }

    SendHandle* Manager::open_send(OpenSendData data) {
        auto handle = std::make_unique<SendHandle>(unique_id(), data);
        SendHandle* raw_handle = handle.get();
        m_router.register_send_publisher(std::move(handle));
        return raw_handle;
    }

    void Manager::send_label(SendHandle* handle, void* buf, size_t buf_size, size_t buf_offset) {
        if (handle == nullptr) {
            ERR_PRINT(__func__, "(): got null send handle");
            return;
        }

        void* src_addr = nullptr;
        uint8_t* data_buf = nullptr;
        size_t data_size = 0;
        size_t data_offset = 0;
        
        // figure out which buffer to use
        if (buf != nullptr) {
            src_addr = buf;
            data_buf = reinterpret_cast<uint8_t*>(buf);
            data_size = buf_size;
            data_offset = buf_offset;
        } else {
            src_addr = handle->data->buf;
            data_buf = handle->data->buf;
            data_size = handle->data->buf_size;
            data_offset = handle->data->buf_offset;
        }

        if (src_addr == nullptr) {
            ERR_PRINT(__func__, "(): got null source buffer");
            return;
        }

        if (data_size == 0) {
            ERR_PRINT(__func__, "(): got data size: 0");
            return;
        }

        if (data_buf == nullptr) {
            ERR_PRINT(__func__, "(): got null data buffer");
            return;
        }

        // create send buffer
        SendBuf sbuf(src_addr, data_size);

        // attach header for send
        LabelHeader hdr = get_send_header(m_id, handle->data->label, data_size);
        std::memcpy(sbuf.data.get(), &hdr, sizeof(hdr));

        // copy data into buffer after header
        std::memcpy(sbuf.data.get() + sizeof(hdr), data_buf + data_offset, data_size);
        
        // hand off to sender
        m_comms.send_label(handle->uid, handle->data->label, std::move(sbuf));
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
        auto result = m_broadcast.open_and_join(m_cfg.mcast_cfg);
        if (result.code != sock::SockErr::None) {
            ERR_PRINT("udp multicast group join failed, config dump:");
            ERR_PRINT("    group ip=   ", m_cfg.mcast_cfg.group_ip, ":", m_cfg.mcast_cfg.port);
            ERR_PRINT("    bind ip=    ", m_cfg.mcast_cfg.bind_ip);
            ERR_PRINT("    ttl=        ", m_cfg.mcast_cfg.ttl);
            ERR_PRINT("    loopback=   ", m_cfg.mcast_cfg.loopback);
            ERR_PRINT("    reuse_addr= ", m_cfg.mcast_cfg.reuse_addr);
            ERR_PRINT("SOCKET ERROR DETAILS:");
            print_socket_result(result);
            return false;
        }

        std::thread([this]() {
            while (true) {
                send_broadcast();
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        }).detach();

        std::thread([this]() {
            //plat::affinitize_current_thread(2);
            while (true) { recv_broadcast(); }
        }).detach();
        
        LOG("udp multicast group joined, broadcast send/recv threads started");
        return true;
    }

    void Manager::send_broadcast() {
        evtlog::info(elog_kind::Send_Start, elog_cat::Broadcast);
        BroadcastMessage msg;
        msg.id = m_id;
        msg.send_labels = m_router.get_send_labels();
        msg.recv_labels = m_router.get_recv_labels();

        auto err = m_broadcast.send_broadcast(&msg, sizeof(msg));
        if (err.code != sock::SockErr::None) {
            evtlog::warn(elog_kind::Send_Failed, elog_cat::Broadcast);
            ERR_PRINT("broadcast send failed!");
            print_socket_result(err);
        }

        evtlog::info(elog_kind::Send_End, elog_cat::Broadcast);
    }

    void Manager::recv_broadcast() {
        BroadcastMessage msg;
        m_broadcast.recv_broadcast(&msg, sizeof(msg));
        if (msg.id == m_id) return;
        
        evtlog::info(elog_kind::Recv_Start, elog_cat::Broadcast);
        
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

        evtlog::info(elog_kind::Recv_End, elog_cat::Broadcast);
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
                    evtlog::info(elog_kind::AddLocalSendSubscriber, elog_cat::Router, source_id, info.label);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("adding remote send subscriber, nodeid=", source_id, " label=", info.label);
                    m_router.add_remote_send_subscriber(info.label, info.size, source_id);
                    evtlog::info(elog_kind::AddRemoteSendSubscriber, elog_cat::Router, source_id, info.label);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add send subscriber but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }
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
                    evtlog::info(elog_kind::RemoveLocalSendSubscriber, elog_cat::Router, source_id, label);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("removing remote send subscriber, nodeid=", source_id, " label=", label);
                    m_router.remove_remote_send_subscriber(label, source_id);
                    evtlog::info(elog_kind::RemoveRemoteSendSubscriber, elog_cat::Router, source_id, label);
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
                    evtlog::info(elog_kind::AddLocalRecvPublisher, elog_cat::Router, source_id, info.label);
                    m_comms.start_local_recv_worker(info.label, info.size);
                    break;
                }
                case addr::RouteKind::Socket: {
                    // add remote recv pub, recv worker should already be listening to this socket
                    PRINT("adding remote recv publisher, nodeid=", source_id, " label=", info.label);
                    m_router.add_remote_recv_publisher(info.label, info.size, source_id);
                    evtlog::info(elog_kind::AddRemoteRecvPublisher, elog_cat::Router, source_id, info.label);
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
                    evtlog::info(elog_kind::RemoveLocalRecvPublisher, elog_cat::Router, source_id, label);
                    m_comms.stop_local_recv_worker(label);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("removing remote recv publisher, nodeid=", source_id, " label=", label);
                    m_router.remove_remote_recv_publisher(label,source_id);
                    evtlog::info(elog_kind::RemoveRemoteRecvPublisher, elog_cat::Router, source_id, label);
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