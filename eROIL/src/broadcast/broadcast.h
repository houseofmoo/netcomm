#pragma once

#include <array>
#include <mutex>
#include <thread>
#include <chrono>

#include "socket/udp_multicast.h"
#include "types/types.h"
#include "router/router.h"
#include "address/address.h"

namespace eroil {
    struct LabelInfo {
        Label label;
        uint32_t size;
    };

    struct BroadcastMessage {
        int id;
        std::array<LabelInfo, MAX_LABELS> send_labels;
        std::array<LabelInfo, MAX_LABELS> recv_labels;
    };

    class Broadcast {
        private:
            NodeId m_id;
            Address& m_address_book;
            Router& m_router;
            sock::UDPMulticastSocket m_udp;
            uint8_t m_port;

        public:
            Broadcast(NodeId id, Address& address, Router& router);
            ~Broadcast();

            bool start_broadcast(uint16_t port);
            bool is_connected() const;

        private:
            void send_broadcast();
            void recv_broadcast();
    };
}
        