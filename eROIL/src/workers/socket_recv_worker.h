#pragma once

#include "types/types.h"
#include "router/router.h"
#include "net/socket.h"
#include "thread_worker.h"

namespace eroil::worker {
    enum class WireFlags : uint16_t {
        Data = 1 << 0,
        Connect = 1 << 1,
        Disconnect = 1 << 2
    };

    inline void set_flag(uint16_t& flags, const WireFlags flag) { flags |= static_cast<uint16_t>(flag); }
    inline bool has_flag(const uint16_t flags, const WireFlags flag) { return flags & static_cast<uint16_t>(flag); }

    #pragma pack(push, 1)
    struct WireHeader {
        uint32_t magic;
        uint16_t version;
        uint16_t flags;
        uint32_t label;
        uint32_t data_size;
    };
    #pragma pack(pop)

    static constexpr uint32_t kMagic = 0x4C4F5245u; // 'EROL'
    static constexpr uint16_t kWireVer = 1;
    static constexpr size_t kMaxPayload = 1u << 20; // 1 MB

    class SocketRecvWorker final : public ThreadWorker {
        private:
            Router& m_router;
            NodeId m_peer_id;
            std::shared_ptr<net::ClientSocket> m_sock;

        public:
            SocketRecvWorker(Router& router, NodeId peer_id, std::shared_ptr<net::ClientSocket> sock);
            void launch();

            SocketRecvWorker(const SocketRecvWorker&) = delete;
            SocketRecvWorker& operator=(const SocketRecvWorker&) = delete;

        protected:
            void on_stopped() override;
            void request_unblock() override;
            void run() override;

        private:
            bool recv_exact(void* dst, size_t n);
    };
}

