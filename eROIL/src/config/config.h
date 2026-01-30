#pragma once
#include <string>
#include "types/const_types.h"

namespace eroil::cfg {
    enum class ManagerMode {
        Normal,
        TestMode_Local_ShmOnly,    // sets all nodes found in peer_ips.cfg to shared memory
        TestMode_Local_SocketOnly, // sets all nodes found in peer_ips.cfg to socket
        TestMode_Sim_Network       // sets half of nodes found in peer_ips.cfg to socket, other half to shared memory
    };

    // udp mcast configuration
    struct UdpMcastConfig {
        std::string group_ip = "239.255.0.1"; // admin scope...which may not work
        uint16_t port = 30001;
        std::string bind_ip = "0.0.0.0"; // INADDR_ANY
        int ttl = 1;
        bool loopback = true;
        bool reuse_addr = true;
    };

    // manager configuration
    struct ManagerConfig {
        NodeId id = 0;
        ManagerMode mode = ManagerMode::Normal;
        UdpMcastConfig mcast_cfg{};
    };

    ManagerConfig get_manager_cfg(int id);
}