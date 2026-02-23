#pragma once
#include <vector>
#include <string>
#include <unordered_map>

#include "types/const_types.h"
#include "config/config.h"
#include "address/address.h"
#include "router/router.h"
#include "socket/udp_multicast.h"
#include "socket/socket_context.h"
#include "comm/connection_manager.h"
#include "macros.h"
#include "log/evtlog_api.h"

namespace eroil {

    /*
        NOTES:
            1) currently we do not check if an existing shared memory block is the correct/expected 
            size on windows since windows does not have a clean way of doing this that is reliable.
            this is checked on linux builds since there is a api to do this reliably.

        TODO:
            1) Right now, if a node dies, there is the potential of of writers to start complaining cause
            the shared memory block will quickly run out of memory and then all senders will start spamming
            errros about not enough space in shared memory block. we need a way to throttle senders in this case
            until the node is resumed
    */

    class Manager {
        private:
            NodeId m_id;
            cfg::ManagerConfig m_cfg;
            rt::Router m_router;
            sock::SocketContext m_sock_context;
            comm::ConnectionManager m_comms;
            sock::UDPMulticastSocket m_broadcast;
            bool m_valid;

        public:
            Manager(cfg::ManagerConfig cfg);
            ~Manager() = default;

            EROIL_NO_COPY(Manager)
            EROIL_NO_MOVE(Manager)

            bool init();
            hndl::SendHandle* open_send(hndl::OpenSendData data);
            void send_label(hndl::SendHandle* handle, std::byte* buf, size_t buf_size, size_t send_offset, size_t recv_offset);
            void close_send(hndl::SendHandle* handle);
            hndl::RecvHandle* open_recv(hndl::OpenReceiveData data);
            void close_recv(hndl::RecvHandle* handle);
            void write_event_log() noexcept { evtlog::write_evtlog(); }
            void write_event_log(const std::string& directory) noexcept { evtlog::write_evtlog(directory); }

        private:
            bool start_broadcast();
            void send_broadcast();
            void recv_broadcast();
            void add_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels);
            void remove_subscriber(const NodeId source_id, const std::unordered_map<Label, uint32_t>& recv_labels);
    };
}