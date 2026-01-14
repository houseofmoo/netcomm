#pragma once
#include <string_view>
#include <string>
#include <vector>
#include "types/types.h"

namespace eroil {
    constexpr std::string_view MANAGE_CONFIG_FILE_PATH = "etc/manager_ips.txt";
    constexpr const char* LOCAL_HOST = "127.0.0.1";
    constexpr uint16_t PORT_START = 8080;

    struct NodeInfo {
        NodeId id;
        std::string ip;
        uint16_t port;
    };

    // read ip addr config file and return results
    std::vector<NodeInfo> GetNodeInfo(const std::string_view& filename);

    // generate ip addrs using 127.0.0.1:8080+id
    std::vector<NodeInfo> GetNodeInfo();

    // manager configuration
    enum class ManagerMode {
        Normal,
        TestMode_Local_ShmOnly,     // uses fake config file and sets all node routes to shm (all nodes much be local)
        TestMode_Lopcal_SocketOnly, // uses fake config file and sets all node routes to socket (all nodes much be local)
        TestMode_Sim_Network        // uses fake config file and generate a simulated network of nodes on sockets and shm (all nodes much be local)
    };

    struct ManagerConfig {
        NodeId id = 0;
        ManagerMode mode = ManagerMode::Normal;
        std::vector<NodeInfo> nodes = {};
    };

    // build manager cfg based on manager mode
    ManagerConfig get_manager_cfg(int id, ManagerMode mode);
}