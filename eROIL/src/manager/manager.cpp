#include "manager.h"
#include <array>
#include <cstring>
#include <thread>
#include <chrono>
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
        uint16_t flags = 0;
        set_flag(flags, LabelFlag::Data);

        LabelHeader hdr;
        hdr.magic = MAGIC_NUM;
        hdr.version = VERSION;
        hdr.source_id = m_id;
        hdr.flags = flags;
        hdr.label = handle->data->label;
        hdr.data_size = data_size;

        std::memcpy(sbuf.data.get(), &hdr, sizeof(hdr));

        // copy data into buffer after header
        std::memcpy(sbuf.data.get() + sizeof(hdr), data_buf + data_offset, data_size);
        
        // hand off to sender
        m_comms.send_label(handle->uid, handle->data->label, std::move(sbuf));
    }

    void Manager::close_send(SendHandle* handle) {
              if (handle == nullptr) {
            ERR_PRINT(__func__, "(): got handle that was nullptr");
            return;
        }
        
        m_router.unregister_send_publisher(handle);
    }

    RecvHandle* Manager::open_recv(OpenReceiveData data) {
        // TODO: validate open recv data

        auto handle = std::make_unique<RecvHandle>(unique_id(), data);
        RecvHandle* raw_handle = handle.get();
        m_router.register_recv_subscriber(std::move(handle));
        return raw_handle;
    }

    void Manager::close_recv(RecvHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT(__func__, "(): got handle that was nullptr");
            return;
        }

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
            send_broadcast();
        }).detach();

        std::thread([this]() {
            //plat::affinitize_current_thread(2);
            recv_broadcast();
        }).detach();
        
        LOG("udp multicast group joined, broadcast send/recv threads started");
        return true;
    }

    void Manager::send_broadcast() {
        BroadcastMessage msg{};
        msg.id = m_id;

        while (true) {
            evtlog::info(elog_kind::Send_Start, elog_cat::Broadcast);
            
            msg.send_labels = m_router.get_send_labels_snapshot();
            msg.recv_labels = m_router.get_recv_labels_snapshot();

            // send broadcast
            auto err = m_broadcast.send_broadcast(&msg, sizeof(msg));
            if (err.code != sock::SockErr::None) {
                evtlog::warn(elog_kind::Send_Failed, elog_cat::Broadcast);
                ERR_PRINT("broadcast send failed!");
                print_socket_result(err);
            }

            evtlog::info(elog_kind::Send_End, elog_cat::Broadcast);
            std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
        }
    }

    void Manager::recv_broadcast() {
        BroadcastMessage msg{};

        while (true) {
            m_broadcast.recv_broadcast(&msg, sizeof(msg));
            if (msg.id == m_id) continue;
            
            evtlog::info(elog_kind::Recv_Start, elog_cat::Broadcast);
            // time how long this takes
            //time::Timer t("recv_broadcast()");

            std::unordered_map<Label, uint32_t> recv{};
            recv.reserve(MAX_LABELS);
            for (const auto& recv_label : msg.recv_labels.labels) {
                if (recv_label.label > INVALID_LABEL) {
                    auto [it, inserted] = recv.emplace(recv_label.label, recv_label.size);
                    if (!inserted) {
                        (void)it; // dont care about iterator
                        ERR_PRINT("we got a duplicate in recv'd broadcast message recv_label list");
                        evtlog::warn(elog_kind::DuplicateLabel, elog_cat::Broadcast);
                    }
                }
            }
            add_subscriber(msg.id, recv);
            remove_subscriber(msg.id, recv);

            std::unordered_map<Label, uint32_t> send{};
            send.reserve(MAX_LABELS);
            for (const auto& send_label : msg.send_labels.labels) {
                if (send_label.label > INVALID_LABEL) {
                    auto [it, inserted] = send.emplace(send_label.label, send_label.size);
                    if (!inserted) {
                        (void)it; // dont care about iterator
                        ERR_PRINT("we got a duplicate in recv'd broadcast message send_labels list");
                        evtlog::warn(elog_kind::DuplicateLabel, elog_cat::Broadcast);
                    }
                }
            }
            add_publisher(msg.id, send);
            remove_publisher(msg.id, send);

            evtlog::info(elog_kind::Recv_End, elog_cat::Broadcast);
        }
    }

    void Manager::add_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels) {
        for (const auto& info : recv_labels) {
            const auto& label = info.first;
            const auto& size = info.second;
            
            if (label <= INVALID_LABEL) continue;

            // if we do not send this label, ignore it
            if (!m_router.has_send_route(label)) continue;

            // check if they're already on subscriber list
            if (m_router.is_send_subscriber(label, source_id)) continue;

            // add them as a subscriber of this label
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Shm: {
                    PRINT("adding local send subscriber, nodeid=", source_id, " label=", label);
                    m_router.add_local_send_subscriber(label, size, m_id, source_id);
                    evtlog::info(elog_kind::AddLocalSendSubscriber, elog_cat::Router, source_id, label);
                    break;
                }
                case addr::RouteKind::Socket: {
                    PRINT("adding remote send subscriber, nodeid=", source_id, " label=", label);
                    m_router.add_remote_send_subscriber(label, size, source_id);
                    evtlog::info(elog_kind::AddRemoteSendSubscriber, elog_cat::Router, source_id, label);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add send subscriber but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }
        }
    }

    void Manager::remove_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels) {
        // all the labels we send
        auto send_labels = m_router.get_send_labels();
        for (const auto& [label, _] : send_labels) {
            if (label <= INVALID_LABEL) continue; // should never occur

            // if theyre not a recver of this label, continue
            if (!m_router.is_send_subscriber(label, source_id)) continue;

            // they still want this label, continue
            if (recv_labels.find(label) != recv_labels.end()) continue;

            // they no longer subscribe to this label, remove them
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

    void Manager::add_publisher(const NodeId source_id, const std::unordered_map<Label, uint32_t>& send_labels) {
        for (const auto& info : send_labels) {
            const auto& label = info.first;
            const auto& size = info.second;
            
            if (label <= INVALID_LABEL) continue;

            // if we do not want this label, continue
            if (!m_router.has_recv_route(label)) continue;

            // if already subscribed, continue
            if (m_router.is_recv_publisher(label, source_id)) continue;

            // this is a label we want to recv and have not registered a publisher
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Shm: {
                    // add local recv pub and start recv worker to watch shared memory block
                    PRINT("adding local recv publisher, nodeid=", source_id, " label=", label);
                    m_router.add_local_recv_publisher(label, size, m_id, source_id);
                    evtlog::info(elog_kind::AddLocalRecvPublisher, elog_cat::Router, source_id, label);
                    m_comms.start_local_recv_worker(label, size);
                    break;
                }
                case addr::RouteKind::Socket: {
                    // add remote recv pub, recv worker should already be listening to this socket
                    PRINT("adding remote recv publisher, nodeid=", source_id, " label=", label);
                    m_router.add_remote_recv_publisher(label, size, source_id);
                    evtlog::info(elog_kind::AddRemoteRecvPublisher, elog_cat::Router, source_id, label);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add recv publisher but did not know routekind, ", (int)addr.kind);
                    break;
                }
            }
        }
    }

    void Manager::remove_publisher(const NodeId source_id, const std::unordered_map<Label, uint32_t>& send_labels) {
        // all the labels we recv
        auto recv_labels = m_router.get_recv_labels();
        for (const auto& [label, _] : recv_labels) {
            if (label <= INVALID_LABEL) continue; // should never occur

            // are they a publisher of this label, if not continue
            if (!m_router.is_recv_publisher(label, source_id)) continue;

            // do they still publish this label, if so continue
            if (send_labels.find(label) != send_labels.end()) continue;

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