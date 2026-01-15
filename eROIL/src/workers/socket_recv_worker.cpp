#include "socket_recv_worker.h"
#include <eROIL/print.h>
#include "types/types.h"
#include "address/address.h"

namespace eroil::worker {
    SocketRecvWorker::SocketRecvWorker(Router& router, NodeId id, NodeId peer_id)
        : m_router(router), m_id(id), m_peer_id(peer_id), m_is_active(false), m_sock(nullptr) {
            m_sock = m_router.get_socket(m_peer_id);
        }

    void SocketRecvWorker::run() {
        m_is_active = true;
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

                if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                    ERR_PRINT("socket recv got a header that did not have the correct magic and/or version");
                    continue;
                }
                
                if (hdr.data_size > SOCKET_DATA_MAX_SIZE) {
                    ERR_PRINT(
                        "socket recv got a header that indicated data size is > ", SOCKET_DATA_MAX_SIZE,
                        " label=", hdr.label, ", sourceid=", hdr.source_id
                    );
                    break;
                }
                
                if (hdr.data_size <= 0 && has_flag(hdr.flags, LabelFlag::Data)) {
                    ERR_PRINT(
                        "socket recv got a header that indicated data size is 0, label=", hdr.label,
                        ", sourceid=", hdr.source_id
                    );
                    continue;
                }

                if (!has_flag(hdr.flags, LabelFlag::Data)) {
                    if (!has_flag(hdr.flags, LabelFlag::Ping)) {
                        ERR_PRINT("got a hdr that indicated it was not a ping or data, hdr.flag=", hdr.flags);
                    }
                    continue;
                }

                payload.resize(hdr.data_size);
                if (!recv_exact(payload.data(), payload.size())) {
                    ERR_PRINT("socket recv failed to get expected data size, size=", hdr.data_size);
                    break;
                }
                if (stop_requested()) break;

                m_router.recv_from_publisher(static_cast<Label>(hdr.label), payload.data(), payload.size());
            }
        } catch (const std::exception& e) {
            ERR_PRINT("socket recv exception, attempting re-connect: ", e.what());
        } catch (...) {
            ERR_PRINT("unknown socket recv exception, re-connect");
        }
        m_is_active = false;
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

