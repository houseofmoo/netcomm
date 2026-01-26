#pragma once
#include <unordered_map>
#include <memory>
#include "address/address.h"
#include "router/router.h"
#include "socket/tcp_socket.h"
#include "workers/send_worker.h"
#include "workers/socket_recv_worker.h"
#include "workers/shm_recv_worker.h"
#include "types/types.h"

namespace eroil {
    class ConnectionManager {
        private:
            NodeId m_id;
            Router& m_router;
            sock::TCPServer m_tcp_server;

            worker::SendWorker m_sender;
            worker::ShmRecvWorker m_shm_recvr;
            std::unordered_map<NodeId, std::unique_ptr<worker::SocketRecvWorker>> m_sock_recvrs;

        public:
            ConnectionManager(NodeId id, Router& router);
            ~ConnectionManager() = default;

            bool start();
            void send_label(handle_uid uid, Label label, io::SendBuf send_buf);
            void start_remote_recv_worker(NodeId from_id);
            void stop_remote_recv_worker(NodeId from_id);

        private:
            void run_tcp_server();
            void remote_connection_monitor();
            bool connect_to_remote_peer(addr::NodeAddress peer_info);
            void ping_remote_peer(addr::NodeAddress peer_info, std::shared_ptr<sock::TCPClient> client);
            bool send_id(sock::TCPClient* sock);
            bool send_ping(sock::TCPClient* sock);
    };
}