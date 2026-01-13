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
            NodeId m_id;
            NodeId m_peer_id;
            bool m_is_active;
            std::shared_ptr<sock::TCPClient> m_sock;

        public:
            SocketRecvWorker(Router& router, NodeId id, NodeId peer_id);

            // do not copy
            SocketRecvWorker(const SocketRecvWorker&) = delete;
            SocketRecvWorker& operator=(const SocketRecvWorker&) = delete;

        protected:
            void run() override;

        private:
            bool recv_exact(void* dst, size_t n);
    };
}

