#include "socket_recv_worker.h"
#include <eROIL/print.h>
#include "types/types.h"
#include "address/address.h"
#include "log/evtlog_api.h"

namespace eroil::worker {
    SocketRecvWorker::SocketRecvWorker(Router& router, NodeId id, NodeId peer_id)
        : m_router(router), m_id(id), m_peer_id(peer_id), m_sock(nullptr) {
            m_sock = m_router.get_socket(m_peer_id);
        }

    void SocketRecvWorker::run() {
        try {
            std::vector<std::byte> payload;
            while (!stop_requested()) {
                LabelHeader hdr{};

                if (!m_sock->is_connected()) break;
                
                if (!recv_exact(&hdr, sizeof(hdr))) {
                    ERR_PRINT("socket recv failed to get expected header size");
                    break;
                }
                
                if (stop_requested()) break;
                evtlog::info(elog_kind::SocketRecvWorker_Start, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);

                if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                    ERR_PRINT("socket recv got a header that did not have the correct magic and/or version");
                    evtlog::warn(elog_kind::SocketRecvWorker_Warning, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    evtlog::info(elog_kind::SocketRecvWorker_End, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    continue;
                }
                
                if (hdr.data_size > SOCKET_DATA_MAX_SIZE) {
                    ERR_PRINT(
                        "socket recv got a header that indicated data size is > ", SOCKET_DATA_MAX_SIZE,
                        " label=", hdr.label, ", sourceid=", hdr.source_id
                    );
                    evtlog::error(elog_kind::SocketRecvWorker_Error, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    break;
                }
                
                if (hdr.data_size <= 0 && has_flag(hdr.flags, LabelFlag::Data)) {
                    ERR_PRINT(
                        "socket recv got a header that indicated data size is 0, label=", hdr.label,
                        ", sourceid=", hdr.source_id
                    );
                    evtlog::warn(elog_kind::SocketRecvWorker_Warning, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    evtlog::info(elog_kind::SocketRecvWorker_End, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    continue;
                }

                if (!has_flag(hdr.flags, LabelFlag::Data)) {
                    if (!has_flag(hdr.flags, LabelFlag::Ping)) {
                        ERR_PRINT("got a hdr that indicated it was not a ping or data, hdr.flag=", hdr.flags);
                    }
                    evtlog::warn(elog_kind::SocketRecvWorker_Warning, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    evtlog::info(elog_kind::SocketRecvWorker_End, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    continue;
                }

                payload.resize(hdr.data_size);
                if (!recv_exact(payload.data(), payload.size())) {
                    ERR_PRINT("socket recv failed to get expected data size, size=", hdr.data_size);
                    evtlog::error(elog_kind::SocketRecvWorker_Error, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
                    break;
                }
                if (stop_requested()) break;

                m_router.recv_from_publisher(static_cast<Label>(hdr.label), payload.data(), payload.size());
                evtlog::info(elog_kind::SocketRecvWorker_End, elog_cat::Worker, hdr.label, hdr.data_size, m_peer_id);
            }
        } catch (const std::exception& e) {
            ERR_PRINT("socket recv exception, attempting re-connect: ", e.what());
        } catch (...) {
            ERR_PRINT("unknown socket recv exception, re-connect");
        }

        evtlog::info(elog_kind::SocketRecvWorker_Exit, elog_cat::Worker, 0, 0, m_peer_id);
        PRINT("socket recv worker for nodeid=", m_peer_id, " exits");
    }

    bool SocketRecvWorker::recv_exact(void* dst, size_t size) {
        if (stop_requested()) return false;
        auto result = m_sock->recv_all(dst, size);
        switch (result.code) {
            case sock::SockErr::None: {
                if (result.bytes <= 0) return false; // should never happen
                break;
            }

            case sock::SockErr::WouldBlock: {
                // should never happen with blocking sockets
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                break;
            }

            case sock::SockErr::Closed:     // fallthrough
            case sock::SockErr::Unknown:    // fallthrough
            default: return false;
        }
        return true;
    }
}

