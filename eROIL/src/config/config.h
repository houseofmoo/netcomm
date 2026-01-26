#pragma once
#include <string_view>
#include <string>
#include <unordered_map>
#include "types/types.h"

namespace eroil::cfg {
    enum class ManagerMode {
        Normal,
        TestMode_Local_ShmOnly,    // uses fake config file and sets all node routes to shm (all nodes much be local)
        TestMode_Local_SocketOnly, // uses fake config file and sets all node routes to socket (all nodes much be local)
        TestMode_Sim_Network       // uses fake config file and generate a simulated network of nodes on sockets and shm (all nodes much be local)
    };

    // udp mcast configuration
    struct UdpMcastConfig {
        std::string group_ip = "239.255.0.1"; // administratively-scoped example
        uint16_t port = 30001;
        std::string bind_ip = "0.0.0.0"; // INADDR_ANY
        int ttl = 1;
        bool loopback = true;
        bool reuse_addr = true;

        // optional: choose NIC for multicast (Windows uses interface index or local IP)
        // let OS choose default route for now
    };

    // manager configuration
    struct ManagerConfig {
        NodeId id = 0;
        ManagerMode mode = ManagerMode::Normal;
        UdpMcastConfig mcast_cfg{};
    };

    ManagerConfig get_manager_cfg(int id);
}