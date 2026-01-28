#include "manager.h"
#include <array>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>
#include <eROIL/print.h>
#include "types/const_types.h"
#include "timer/timer.h"
#include "platform/platform.h"
#include "log/evtlog_api.h"
#include "types/label_io_types.h"

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
        auto addr = addr::get_address(m_id);
        if (addr.kind == addr::RouteKind::None) {
            ERR_PRINT("manager has no entry in nodes info list, manager cannot initialize");
            m_valid = false;
            return;
        }

        m_valid = true;
        return;
    }

    bool Manager::init() {
        if (!m_valid) {
            return false;
        }

        // estimate tsc frequency (100 ms sleep)
        std::thread([]{
            plat::affinitize_current_thread_to_current_cpu();
            uint64_t tsc_hz = evtlog::estimate_tsc_hz();
            PRINT("tsc frequency estimated: ", tsc_hz, " Hz");
        }).join();

        if (!m_comms.start()) {
            return false;
        }

        if (!start_broadcast()) {
            return false;
        }

        // wait about 1 minute then dump time info
        //time::time_log.timed_run(m_id, 60 * 1000);

        return true;
    }

    hndl::SendHandle* Manager::open_send(hndl::OpenSendData data) {
        if (data.buf_size > MAX_LABEL_SEND_SIZE) {
            ERR_PRINT(" got label size=", data.buf_size, " which is bigger than max=", MAX_LABEL_SEND_SIZE);
            ERR_PRINT("ignored open_send for label=", data.label);
            return nullptr;
        }

        auto handle = std::make_unique<hndl::SendHandle>(unique_id(), data);
        hndl::SendHandle* raw_handle = handle.get();
        m_router.register_send_publisher(std::move(handle));
        return raw_handle;
    }

    void Manager::send_label(hndl::SendHandle* handle, std::byte* buf, size_t buf_size, size_t send_offset, size_t recv_offset) {
        if (handle == nullptr) {
            ERR_PRINT("(): got null send handle");
            return;
        }

        // if this is not an offset send, 0 the offset sizes
        if (!handle->data->is_offset) {
            // TODO: send offset 0 here may be wrong
            send_offset = 0;
            recv_offset = 0;
        }

        size_t data_size = 0;
        std::byte* data_buf = nullptr;
        if (buf == nullptr) {
            data_buf = handle->data->buf;
            data_size = handle->data->buf_size;
        } else {
            data_buf = buf;
            data_size = buf_size;
        }

        if (data_size <= 0 || data_size != handle->data->buf_size) {
            ERR_PRINT("(): got data size=", data_size, " that does not match expected size=", handle->data->buf_size);
            return;
        }

        if (data_buf == nullptr) {
            ERR_PRINT("(): got null data buffer");
            return;
        }

        // create send buffer
        io::SendBuf sbuf(data_buf, data_size);

        // attach header for send
        io::LabelHeader hdr;
        hdr.magic = MAGIC_NUM;
        hdr.version = VERSION;
        hdr.source_id = m_id;
        hdr.flags = static_cast<uint16_t>(io::LabelFlag::Data);
        hdr.label = handle->data->label;
        hdr.data_size = static_cast<uint32_t>(data_size);
        hdr.recv_offset = recv_offset;

        // copy header
        std::memcpy(sbuf.data.get(), &hdr, sizeof(hdr));

        // copy data into buffer after header
        std::memcpy(sbuf.data.get() + sizeof(hdr), data_buf + send_offset, data_size);
        
        // hand off to sender
        m_comms.enqueue_send(handle->uid, handle->data->label, std::move(sbuf));
    }

    void Manager::close_send(hndl::SendHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT("(): got handle that was nullptr");
            return;
        }
        
        m_router.unregister_send_publisher(handle);
    }

    hndl::RecvHandle* Manager::open_recv(hndl::OpenReceiveData data) {
        auto handle = std::make_unique<hndl::RecvHandle>(unique_id(), data);
        hndl::RecvHandle* raw_handle = handle.get();
        m_router.register_recv_subscriber(std::move(handle));
        return raw_handle;
    }

    void Manager::close_recv(hndl::RecvHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT("(): got handle that was nullptr");
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
        io::BroadcastMessage msg{};
        msg.id = m_id;

        while (true) {
            EvtMark mark(elog_cat::Broadcast);
            
            msg.send_labels = m_router.get_send_labels_snapshot();
            msg.recv_labels = m_router.get_recv_labels_snapshot();

            // send broadcast
            auto err = m_broadcast.send_broadcast(&msg, sizeof(msg));
            if (err.code != sock::SockErr::None) {
                evtlog::warn(elog_kind::SendFailed, elog_cat::Broadcast);
                ERR_PRINT("broadcast send failed!");
                print_socket_result(err);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
        }
    }

    void Manager::recv_broadcast() {
        io::BroadcastMessage msg{};

        while (true) {
            m_broadcast.recv_broadcast(&msg, sizeof(msg));
            EvtMark mark(elog_cat::Broadcast);
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
        }
    }

    void Manager::add_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels) {
        for (const auto& [label, size] : recv_labels) {
            if (label <= INVALID_LABEL) continue;

            // if we do not send this label, ignore it
            if (!m_router.has_send_route(label)) continue;

            // check if they're already on subscriber list
            if (m_router.is_send_subscriber(label, source_id)) continue;

            // add them as a subscriber of this label
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Self: // fallthrough
                case addr::RouteKind::Shm: {
                    PRINT("adding local send subscriber, nodeid=", source_id, " label=", label);
                    m_router.add_local_send_subscriber(label, size, source_id);
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
        auto send_labels = m_router.get_send_labels();
        for (const auto& [label, _] : send_labels) {
            if (label <= INVALID_LABEL) continue;

            // if theyre not a recver of this label, continue
            if (!m_router.is_send_subscriber(label, source_id)) continue;

            // they still want this label, continue
            if (recv_labels.find(label) != recv_labels.end()) continue;

            // they no longer subscribe to this label, remove them
            const auto addr = addr::get_address(source_id);
            switch (addr.kind) {
                case addr::RouteKind::Self: // fallthrough
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
}