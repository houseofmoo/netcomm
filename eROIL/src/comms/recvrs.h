#pragma once
#include <unordered_map>
#include "address/address.h"
#include "router/router.h"
#include "socket/socket_context.h"
#include "socket/tcp_socket.h"
#include "workers/socket_recv_worker.h"
#include "workers/shm_recv_worker.h"

namespace eroil {
    class Comms {
        private:
            NodeId m_id;
            Address& m_address_book;
            Router& m_router;
            sock::SocketContext m_context; // required for sockets to be available
            sock::TCPServer m_tcp_server;
            std::unordered_map<NodeId, worker::SocketRecvWorker> m_sock_recvrs;
            std::unordered_map<Label, worker::ShmRecvWorker> m_shm_recvrs;

        public:
            Comms(NodeId id, Router& router, Address& address);
            ~Comms() = default;
            void start();

        private:
            void start_tcp_server();
            void search_peers();
    };
}