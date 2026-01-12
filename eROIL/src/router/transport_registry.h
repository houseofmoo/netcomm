#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstddef>

#include "types/types.h"
#include "socket/tcp_socket.h"
#include "shm/shm.h"

namespace eroil {
    class TransportRegistry {
        private:
            std::unordered_map<NodeId, std::shared_ptr<sock::TCPClient>> m_sockets;
            std::unordered_map<Label, std::shared_ptr<shm::Shm>> m_send_shm;
            std::unordered_map<Label, std::shared_ptr<shm::Shm>> m_recv_shm;

        public:
            // socket
            bool upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock);
            bool delete_socket(NodeId id);
            std::shared_ptr<sock::TCPClient> get_socket(NodeId id) const;
            bool has_socket(NodeId id) const;

            // send shm
            bool open_send_shm(Label label, size_t label_size);
            bool delete_send_shm(Label label);
            std::shared_ptr<shm::Shm> get_send_shm(Label label);
            bool has_send_shm(Label label);

            // recv shm
            bool open_recv_shm(Label label, size_t label_size);
            bool delete_recv_shm(Label label);
            std::shared_ptr<shm::Shm> get_recv_shm(Label label);
            bool has_recv_shm(Label label);
    };
}
