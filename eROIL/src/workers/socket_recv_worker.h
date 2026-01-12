#pragma once

#include <functional>
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
            std::function<void()> m_on_error;

        public:
            SocketRecvWorker(Router& router, 
                            NodeId peer_id,
                            std::function<void()> on_err);
            SocketRecvWorker(const SocketRecvWorker&) = delete;
            SocketRecvWorker& operator=(const SocketRecvWorker&) = delete;

        protected:
            void request_unblock() override;
            void run() override;

        private:
            bool recv_exact(void* dst, size_t n);
    };
}

