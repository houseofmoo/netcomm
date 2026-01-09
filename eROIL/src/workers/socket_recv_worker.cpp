#include "socket_recv_worker.h"
#include <eROIL/print.h>

namespace eroil::worker {
    SocketRecvWorker::SocketRecvWorker(Router& router, NodeId peer_id, std::shared_ptr<net::ClientSocket> sock)
        : m_router(router), m_peer_id(peer_id), m_sock(std::move(sock)) {}

    void SocketRecvWorker::launch() { 
        start(); 
    }

    void SocketRecvWorker::on_stopped() {
        m_sock->close();
    }

    void SocketRecvWorker::request_unblock() {
        if (m_sock != nullptr) {
            m_sock->shutdown_read();
        }
    }

    void SocketRecvWorker::run() {
        try {
            std::vector<std::byte> payload;
            while (!stop_requested()) {
                WireHeader hdr{};
                // TODO: these may be error states that are unrecoverable, but maybe not?
                if (!recv_exact(&hdr, sizeof(hdr))) break;
                if (hdr.magic != kMagic || hdr.version != kWireVer) break;
                if (hdr.data_size > kMaxPayload) break;
                if (hdr.data_size <= 0) {
                    ERR_PRINT("socket recv got a header that indicated data size was 0");
                    continue;
                }

                payload.resize(hdr.data_size);
                if (!recv_exact(payload.data(), payload.size())) break;

                // TODO: maybe add from peer id to recv from publisher
                m_router.recv_from_publisher(static_cast<Label>(hdr.label), payload.data(), payload.size());
            }
        } catch (const std::exception& e) {
            ERR_PRINT("socket recv exception, worker stopping: ", e.what());
            // TODO: tell router the peer is gone
            // m_router.on_peer_disconnected(m_peer_id);
        } catch (...) {
            ERR_PRINT("unknown socket recv exception, worker stopping: ");
            // TODO: tell router the peer is gone
            // m_router.on_peer_disconnected(m_peer_id);
        }
    }

    bool SocketRecvWorker::recv_exact(void* dst, size_t size) {
        std::byte* dst_offset = static_cast<std::byte*>(dst);
        size_t got = 0;

        while (got < size) {
            if (stop_requested()) return false;

            net::SocketOp op = m_sock->recv(dst_offset + got, size - got);

            switch (op.result) {
                case net::SocketIoResult::Ok: {
                    if (op.bytes <= 0) return false; // should never happen
                    got += static_cast<size_t>(op.bytes);
                    break;
                }

                case net::SocketIoResult::WouldBlock: {
                    // should never happen with blocking sockets
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    break;
                }

                case net::SocketIoResult::Closed: // fallthrough
                case net::SocketIoResult::Error:  // fallthrough
                default: return false;
            }
        }
        return true;
    }
}

