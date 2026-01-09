#pragma once
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>

#include "address/address.h"
#include "broadcast/broadcast.h"
#include "config/config.h"
#include "router/router.h"
#include "types/handles.h"
#include "types/types.h"

#include "shm/shm.h"

namespace eroil {
    
    class Manager {
        private:
            int m_id;
            Address m_address_book;
            Router m_router;
            Broadcast m_broadcast;

        public:
            Manager(int id, std::vector<NodeInfo> nodes);
            ~Manager() = default;

            SendHandle* open_send(OpenSendData data);
            void send_label(SendHandle* handle, void* buf, size_t buf_size, size_t buf_offset);
            void close_send(SendHandle* handle);
            
            RecvHandle* open_recv(OpenReceiveData data);
            void close_recv(RecvHandle* handle);

        private:
            void send_broadcast();
            void recv_broadcast();
    };
}