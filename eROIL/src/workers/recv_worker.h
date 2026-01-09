#pragma once

#include "types/types.h"
#include "router/router.h"
#include "net/socket.h"

namespace eroil::worker {
    struct IRecvWorker {
        virtual ~IRecvWorker() = default;
        virtual void stop() = 0;
    };

    #pragma pack(push, 1)
    struct WireHeader {
        uint32_t magic;     // e.g. 'EROL'
        uint16_t version;   // bump when needed
        uint16_t flags;     // optional
        uint32_t label;     // Label (or hash/enum)
        uint32_t size;      // payload size in bytes
    };
    #pragma pack(pop)
    static constexpr uint32_t kMagic = 0x4C4F5245u; // 'EROL' little-endian

    class SocketRecvWorker {
        public:
            SocketRecvWorker(Router& r,
                            NodeId peer_id,
                            std::shared_ptr<net::ClientSocket> sock)
                : m_router(r), m_peer_id(peer_id), m_sock(std::move(sock)),
                m_thr([this](std::stop_token st){ run(st); })
            {}

            void stop() { m_thr.request_stop(); /* optionally shutdown socket */ }

        private:
            void run(std::stop_token st) {
                std::vector<std::byte> buf;

                while (!st.stop_requested()) {
                    WireHeader hdr{};
                    if (!recv_exact(hdr)) break;

                    if (hdr.magic != kMagic || hdr.version != 1) break;
                    if (hdr.size > kMaxPayload) break;

                    buf.resize(hdr.size);
                    if (!recv_exact_bytes(buf.data(), buf.size())) break;

                    const Label label = static_cast<Label>(hdr.label);
                    //maybe also pass peer_id
                    m_router.recv_from_publisher(label, buf.data(), buf.size());
                }

                // Inform router connection died (optional)
                // m_router.on_peer_disconnected(m_peer_id);
            }

            bool recv_exact(WireHeader& out) {
                return recv_exact_bytes(&out, sizeof(out));
            }

            bool recv_exact_bytes(void* dst, size_t n) {
                std::byte* p = static_cast<std::byte*>(dst);
                size_t got = 0;

                while (got < n) {
                    // replace with your socket wrapper's recv()
                    const auto r = m_sock->recv(p + got, n - got);
                    if (r <= 0) return false; // 0=closed, <0=error
                    got += static_cast<size_t>(r);
                }
                return true;
            }

        private:
            static constexpr size_t kMaxPayload = 1u << 20; // 1 MiB cap (tune)
            Router& m_router;
            NodeId m_peer_id;
            std::shared_ptr<net::ClientSocket> m_sock;
            std::jthread m_thr;
    };

    class ShmLabelRecvWorker {
        public:
            ShmLabelRecvWorker(Router& r,
                            Label label,
                            std::shared_ptr<shm::ShmReceive> shm_recv)
                : m_router(r), m_label(label), m_shm(std::move(shm_recv)),
                m_thr([this](std::stop_token st){ run(st); })
            {}

            void stop() { m_thr.request_stop(); m_shm->interrupt_wait(); }

        private:
            void run(std::stop_token st) {
                std::vector<std::byte> tmp;

                while (!st.stop_requested()) {
                    // Blocks until any writer signals new data for this label.
                    // Return std::optional<NodeId> so we can break cleanly.
                    auto from = m_shm->wait_for_next_publisher(st);
                    if (!from) break;

                    // Option A: shm returns a view (pointer valid until next write)
                    shm::ReadView view = m_shm->read_view(*from);
                    if (!view.ok) continue;

                    // If router fan-out may outlive the view, copy:
                    tmp.resize(view.size);
                    std::memcpy(tmp.data(), view.data, view.size);

                    m_router.recv_from_publisher(*from, m_label, tmp.data(), tmp.size());
                }
            }

        private:
            Router& m_router;
            Label m_label;
            std::shared_ptr<shm::ShmReceive> m_shm;
            std::jthread m_thr;
    };
}