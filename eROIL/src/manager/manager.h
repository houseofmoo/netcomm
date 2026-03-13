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
            size on windows since windows does not have a relaible api from windows. But this is
            checked on linux (linux is the only place is actually matters!)

        TODO:
            1) Right now, if a node dies there is the potential of writers to start erroring due to
            space unavailble. If there is no consumer of the messages written to a shared memory block
            (if a node dies) then the block will fill with messages with no room for new messages. we 
            need a way to throttle senders in this case until the consumer thread for a node is restarted
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