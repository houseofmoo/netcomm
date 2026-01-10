#pragma once
#include <vector>

#include "types/types.h"
#include "types/handles.h"
#include "config/config.h"
#include "address/address.h"
#include "router/router.h"
#include "broadcast/broadcast.h"
#include "comms/recvrs.h"
#include "workers/send_worker.h"

namespace eroil {
    
    class Manager {
        private:
            int m_id;
            Address m_address_book;
            Router m_router;
            Broadcast m_broadcast;
            Comms m_comms;
            worker::SendWorker m_sender;
            bool m_valid;

        public:
            Manager(int id, std::vector<NodeInfo> nodes);
            ~Manager() = default;
            
            bool init();

            SendHandle* open_send(OpenSendData data);
            void send_label(SendHandle* handle, void* buf, size_t buf_size, size_t buf_offset);
            void close_send(SendHandle* handle);
            
            RecvHandle* open_recv(OpenReceiveData data);
            void close_recv(RecvHandle* handle);

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