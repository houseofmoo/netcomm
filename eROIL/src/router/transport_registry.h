#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstddef>

#include "types/types.h"
#include "socket/tcp_socket.h"
#include "shm/shm_recv.h"
#include "shm/shm_send.h"

namespace eroil {
    class TransportRegistry {
        private:
            std::shared_ptr<shm::ShmRecv> m_recv_shm;
            std::unordered_map<NodeId, std::shared_ptr<shm::ShmSend>> m_send_shm;
            std::unordered_map<NodeId, std::shared_ptr<sock::TCPClient>> m_sockets;

        public:
            // socket
            bool upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock);
            bool delete_socket(NodeId id);
            std::shared_ptr<sock::TCPClient> get_socket(NodeId id) const noexcept;
            bool has_socket(NodeId id) const noexcept;
            std::vector<std::shared_ptr<sock::TCPClient>> get_all_sockets() const;

            // send shm
            bool open_send_shm(NodeId dst_id);
            std::shared_ptr<shm::ShmSend> get_send_shm(NodeId dst_id) const noexcept;
            bool has_send_shm(NodeId dst_id) const noexcept;

            // recv shm
            bool open_recv_shm(NodeId my_id);
            std::shared_ptr<shm::ShmRecv> get_recv_shm() const noexcept;
    };
}
