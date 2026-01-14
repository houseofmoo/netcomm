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
            Router m_router;
            sock::SocketContext m_context;
            ConnectionManager m_comms;
            sock::UDPMulticastSocket m_broadcast;
            bool m_valid;

        public:
            Manager(ManagerConfig cfg);
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
            // 1. Spawn send thread (SendWorker)
            // 2. Spawn thread for UDP broadcast handling (send/recv) 2 threads total

            // 3. Spawn thread for TCP Server (1 server thread + N client threads [depends on number of LAN peers])
            //  Each manager needs to open a server listener
            //  Then each manager needs to attempt to connect to other managers that have a LOWER NodeId than itself
            //  and is not a local process (same LAN but different machine)
            //  We'll spawn N threads to make those socket client connections, and once the socket is established,
            //  give the socket to m_router and spawn a SocketRecvWorker to listen to that socket
            
            // 4. on Shm Label recv first creation, spawn ShmRecvWorker (runs on its own thread and hands data to router)
    };
}