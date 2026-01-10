#pragma once

#include "types/types.h"
#include "router/router.h"
#include "socket/tcp_socket.h"
#include "thread_worker.h"

namespace eroil::worker {
    class SocketRecvWorker final : public ThreadWorker {
        private:
            Router& m_router;
            NodeId m_peer_id;
            std::shared_ptr<sock::TCPClient> m_sock;

        public:
            SocketRecvWorker(Router& router, NodeId peer_id, std::shared_ptr<sock::TCPClient> sock);
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

