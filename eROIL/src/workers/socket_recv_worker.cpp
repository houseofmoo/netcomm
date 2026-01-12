#include "socket_recv_worker.h"
#include <eROIL/print.h>
#include "types/label_hdr.h"

namespace eroil::worker {
    SocketRecvWorker::SocketRecvWorker(Router& router, 
                                       NodeId peer_id, 
                                       std::function<void()> on_err)
        : m_router(router), m_peer_id(peer_id), m_sock(nullptr), m_on_error(on_err) {
            m_sock = m_router.get_socket(m_peer_id);
        }

    void SocketRecvWorker::request_unblock() {
        // we NEVER kill a socket ourselves
        // but that means we cannot get unblocked either
    }

    void SocketRecvWorker::run() {
        try {
            std::vector<std::byte> payload;
            while (!stop_requested()) {
                LabelHeader hdr{};

                if (!recv_exact(&hdr, sizeof(hdr))) break;
                if (stop_requested()) break;

                if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                    ERR_PRINT("socket recv got a header that did not have the correct magic and/or version");
                    continue;
                }
                
                if (hdr.data_size > SOCKET_DATA_MAX_SIZE) {
                    ERR_PRINT("socket recv got a header that indicated data size is > ", SOCKET_DATA_MAX_SIZE);
                    break;
                }
                
                if (hdr.data_size <= 0) {
                    ERR_PRINT("socket recv got a header that indicated data size is 0");
                    continue;
                }

                payload.resize(hdr.data_size);
                if (!recv_exact(payload.data(), payload.size())) break;
                if (stop_requested()) break;

                m_router.recv_from_publisher(static_cast<Label>(hdr.label), payload.data(), payload.size());
            }
        } catch (const std::exception& e) {
            ERR_PRINT("socket recv exception, worker stopping: ", e.what());
            m_on_error();
        } catch (...) {
            ERR_PRINT("unknown socket recv exception, worker stopping: ");
            m_on_error();
        }
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

