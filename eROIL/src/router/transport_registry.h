#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstddef>

#include "types/types.h"
#include "net/socket.h"
#include "shm/shm.h"

namespace eroil {
    struct SocketConn {
        NodeId to_id{};
        std::string ip;
        uint16_t port{};
        std::shared_ptr<net::ClientSocket> socket;
    };

    struct SendTargets {
        std::shared_ptr<shm::ShmSend> shm;
        std::vector<std::shared_ptr<net::ClientSocket>> sockets;
        bool has_local = false;
        bool has_remote = false;
    };

    class TransportRegistry {
        private:
            std::unordered_map<NodeId, SocketConn> m_sockets;
            std::unordered_map<Label, std::shared_ptr<shm::ShmSend>> m_send_shm;
            std::unordered_map<Label, std::shared_ptr<shm::ShmRecv>> m_recv_shm;

        public:
            // socket
            bool has_socket(NodeId id) const;
            std::shared_ptr<net::ClientSocket> get_socket(NodeId id) const;
            bool upsert_socket(NodeId id, std::string ip, uint16_t port, std::shared_ptr<net::ClientSocket> sock);

            // send shm
            std::shared_ptr<shm::ShmSend> ensure_send_shm(Label label, size_t size, NodeId to_id);
            // is_remote_send_subscriber() located in route_table
            bool has_local_send_subscriber(Label label, NodeId to_id) const;

            // recv shm
            std::shared_ptr<shm::ShmRecv> ensure_recv_shm(Label label, size_t size, NodeId my_id);
            // is_remote_recv_publisher() located in route_table
            bool has_local_recv_publisher(Label label, NodeId my_id) const;

            SendTargets resolve_send_targets(Label label,
                                            bool want_local,
                                            const std::vector<NodeId>& remote_node_ids) const;

            void erase_send_shm(Label label);
            void erase_recv_shm(Label label);
    };
}
