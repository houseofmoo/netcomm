#pragma once
#include <unordered_map>
#include <memory>
#include "address/address.h"
#include "router/router.h"
#include "socket/tcp_socket.h"
#include "workers/send_worker.h"
#include "workers/socket_recv_worker.h"
#include "workers/shm_recv_worker.h"

namespace eroil {
    class ConnectionManager {
        private:
            NodeId m_id;
            Router& m_router;
            sock::TCPServer m_tcp_server;

            worker::SendWorker m_sender;
            std::unordered_map<NodeId, std::unique_ptr<worker::SocketRecvWorker>> m_sock_recvrs;
            std::unordered_map<Label, std::unique_ptr<worker::ShmRecvWorker>> m_shm_recvrs;

        public:
            ConnectionManager(NodeId id, Router& router);
            ~ConnectionManager() = default;

            void start();
            void send_label(handle_uid uid, Label label, size_t data_size, std::unique_ptr<uint8_t[]> data);
            void start_local_recv_worker(Label label, size_t label_size);
            void start_remote_recv_worker(NodeId from_id);

        private:
            void start_tcp_server();
            void search_remote_peers();
            void monitor_sockets();
            bool send_id(sock::TCPClient* sock);
            bool send_ping(sock::TCPClient* sock);
    };
}