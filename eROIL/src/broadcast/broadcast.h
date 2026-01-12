// #pragma once

// #include <array>
// #include <mutex>
// #include <thread>
// #include <chrono>

// #include "socket/udp_multicast.h" 
// #include "types/types.h"
// #include "address/address.h"
// #include "router/router.h"
// #include "comms/recvrs.h"

// namespace eroil {
//     struct LabelInfo {
//         Label label;
//         uint32_t size;
//     };

//     struct BroadcastMessage {
//         int id;
//         std::array<LabelInfo, MAX_LABELS> send_labels;
//         std::array<LabelInfo, MAX_LABELS> recv_labels;
//     };

//     class Broadcast {
//         private:
//             NodeId m_id;
//             Address& m_address_book;
//             Router& m_router;
//             Comms& m_comms;

//             sock::UDPMulticastSocket m_udp;
//             uint8_t m_port;

//         public:
//             Broadcast(NodeId id, Address& address, Router& router, Comms& comms);
//             ~Broadcast();

//             bool start_broadcast(uint16_t port);
//             bool is_connected() const;

//         private:
//             void send_broadcast();
//             void recv_broadcast();

//             // helpers
//             void add_new_subscribers(const BroadcastMessage& msg);
//             void set_new_publishers(const BroadcastMessage& msg);

//             void remove_subscription(const BroadcastMessage& msg);
//             void remove_publisher(const BroadcastMessage& msg);
//     };
// }
        