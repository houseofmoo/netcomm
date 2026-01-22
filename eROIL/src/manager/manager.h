#pragma once
#include <vector>
#include <unordered_map>

#include "types/types.h"
#include "config/config.h"
#include "address/address.h"
#include "router/router.h"
#include "socket/udp_multicast.h"
#include "socket/socket_context.h"
#include "comm/connection_manager.h"
#include "peer_state.h"

namespace eroil {
    class Manager {
        private:
            NodeId m_id;
            cfg::ManagerConfig m_cfg;
            Router m_router;
            sock::SocketContext m_context;
            ConnectionManager m_comms;
            sock::UDPMulticastSocket m_broadcast;
            bool m_valid;

        public:
            Manager(cfg::ManagerConfig cfg);
            ~Manager() = default;

            bool init();
            hndl::SendHandle* open_send(hndl::OpenSendData data);
            void send_label(hndl::SendHandle* handle, uint8_t* buf, size_t buf_size, size_t send_offset, size_t recv_offset);
            void close_send(hndl::SendHandle* handle);
            hndl::RecvHandle* open_recv(hndl::OpenReceiveData data);
            void close_recv(hndl::RecvHandle* handle);
        
        private:
            bool start_broadcast();
            void send_broadcast();
            void recv_broadcast();
            
            void add_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels);
            void remove_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels);
            void add_publisher(const NodeId source_id, const std::unordered_map<Label, uint32_t>& send_labels);
            void remove_publisher(const NodeId source_id, const std::unordered_map<Label, uint32_t>& send_labels);
    };
}