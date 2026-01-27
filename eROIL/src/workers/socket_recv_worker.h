#pragma once

#include <functional>
#include "types/const_types.h"
#include "router/router.h"
#include "socket/tcp_socket.h"
#include "types/macros.h"

namespace eroil::worker {
    class SocketRecvWorker {
        private:
            Router& m_router;
            NodeId m_id;
            NodeId m_peer_id;
            std::shared_ptr<sock::TCPClient> m_sock;

            std::atomic<bool> m_stop{false};
            std::thread m_thread;

        public:
            SocketRecvWorker(Router& router, NodeId id, NodeId peer_id);
            ~SocketRecvWorker() { stop(); }
            
            EROIL_NO_COPY(SocketRecvWorker)
            EROIL_NO_MOVE(SocketRecvWorker)

            void start();
            void stop();
        
        private:
            bool stop_requested() const { return m_stop.load(std::memory_order_acquire); }
            void run();
            bool recv_exact(void* dst, size_t n);
    };
}

