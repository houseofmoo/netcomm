#pragma once

#include <array>
#include <mutex>
#include "net/socket.h"
#include "types/types.h"

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
            net::socket::UDPMulti m_udp;
            const char* m_ip;
            uint8_t m_port;
            bool m_ok;
            
            std::mutex m_mtx;

        public:
            Broadcast();
            ~Broadcast();

            void setup(uint16_t port);

            void send(const BroadcastMessage& msg);
            void recv(BroadcastMessage& buf) const;
    };
}
        