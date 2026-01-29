#include "socket_recv_worker.h"
#include <eROIL/print.h>
#include "types/const_types.h"
#include "address/address.h"
#include "log/evtlog_api.h"

namespace eroil::wrk {
    SocketRecvWorker::SocketRecvWorker(Router& router, NodeId id, NodeId peer_id) : 
        m_router(router), m_id(id), m_peer_id(peer_id), m_sock(nullptr) {
            m_sock = m_router.get_socket(m_peer_id);
        }

    void SocketRecvWorker::start() {
        if (m_thread.joinable()) return;
        m_stop.store(false, std::memory_order_release);
        m_thread = std::thread([this] { run(); });
    }

    void SocketRecvWorker::stop() {
        //bool was_stopping = m_stop.exchange(true, std::memory_order_acq_rel);
        m_stop.exchange(true, std::memory_order_acq_rel);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void SocketRecvWorker::run() {
        try {
            std::vector<std::byte> payload;
            while (!stop_requested()) {
                io::LabelHeader hdr{};

                if (!m_sock->is_connected()) break;
                
                if (!recv_exact(&hdr, sizeof(hdr))) {
                    ERR_PRINT("socket recv failed to get expected header size");
                    break;
                }
                
                if (stop_requested()) break;
                EvtMark mark(elog_cat::SocketRecvWorker);

                if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                    ERR_PRINT("socket recv got a header that did not have the correct magic and/or version");
                    evtlog::error(elog_kind::InvalidHeader, elog_cat::SocketRecvWorker);
                    continue;
                }
                
                if (hdr.data_size > MAX_LABEL_SEND_SIZE) {
                    ERR_PRINT("socket recv got header that indicates data size is > ", MAX_LABEL_SEND_SIZE);
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id);
                    evtlog::error(elog_kind::InvalidDataSize, elog_cat::SocketRecvWorker, hdr.label, hdr.data_size);
                    continue;
                }
                
                if (hdr.data_size <= 0 && io::has_flag(hdr.flags, io::LabelFlag::Data)) {
                    ERR_PRINT("socket recv got header that indicates data size is 0");
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id);
                    evtlog::error(elog_kind::InvalidDataSize, elog_cat::SocketRecvWorker, hdr.label, hdr.data_size);
                    continue;
                }

                if (!io::has_flag(hdr.flags, io::LabelFlag::Data)) {
                    if (!io::has_flag(hdr.flags, io::LabelFlag::Ping)) {
                        ERR_PRINT("socket recv got header that indicates neither ping or data");
                        ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id, " flags=", hdr.flags);
                    }
                    evtlog::error(elog_kind::InvalidFlags, elog_cat::Worker, hdr.label, hdr.flags);
                    continue;
                }

                payload.resize(hdr.data_size);
                if (!recv_exact(payload.data(), payload.size())) {
                    ERR_PRINT("socket recv failed to get expected data size, size=", hdr.data_size);
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id);
                    evtlog::error(elog_kind::RecvError, elog_cat::SocketRecvWorker);
                    break;
                }
                if (stop_requested()) break;

                m_router.distribute_recvd_label(
                    static_cast<Label>(hdr.label), 
                    payload.data(),
                    payload.size(), 
                    static_cast<size_t>(hdr.recv_offset)
                );
            }
        } catch (const std::exception& e) {
            ERR_PRINT("socket recv exception: ", e.what());
        } catch (...) {
            ERR_PRINT("unknown socket recv exception");
        }

        evtlog::info(elog_kind::Exit, elog_cat::SocketRecvWorker, m_peer_id);
        PRINT("socket recv worker for nodeid=", m_peer_id, " exits");
    }

    bool SocketRecvWorker::recv_exact(void* dst, size_t size) {
        if (stop_requested()) return false;
        sock::SockResult result = m_sock->recv_all(dst, size);
        switch (result.code) {
            case sock::SockErr::None: {
                if (result.bytes <= 0) return false; // should never happen
                break;
            }

            case sock::SockErr::WouldBlock: {
                // should never happen with blocking sockets
                std::this_thread::yield();
                break;
            }

            case sock::SockErr::Closed:     // fallthrough
            case sock::SockErr::Unknown:    // fallthrough
            default: return false;
        }
        return true;
    }
}

