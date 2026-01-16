#pragma once
#include <vector>
#include <array>

#include "types/types.h"
#include "config/config.h"
#include "address/address.h"
#include "router/router.h"
#include "socket/udp_multicast.h"
#include "socket/socket_context.h"
#include "conn/connection_manager.h"

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
            SendHandle* open_send(OpenSendData data);
            void send_label(SendHandle* handle, void* buf, size_t buf_size, size_t buf_offset);
            void close_send(SendHandle* handle);
            RecvHandle* open_recv(OpenReceiveData data);
            void close_recv(RecvHandle* handle);
        
        private:
            bool start_broadcast();
            void send_broadcast();
            void recv_broadcast();
            void add_new_subscribers(const NodeId source_id, const std::vector<LabelInfo>& recv_labels);
            void remove_subscription(const NodeId source_id, const std::vector<LabelInfo>& recv_labels);
            void add_new_publisher(const NodeId source_id, const std::vector<LabelInfo>& send_labels);
            void remove_publisher(const NodeId source_id, const std::vector<LabelInfo>& send_labels);
    };
}