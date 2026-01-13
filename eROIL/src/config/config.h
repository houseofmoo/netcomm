#pragma once
#include <string>
#include <vector>
#include "types/types.h"

namespace eroil {
    constexpr const char* LOCAL_HOST = "127.0.0.1";
    constexpr uint16_t PORT_START = 8080;

    struct NodeInfo {
        NodeId id;
        std::string ip;
        uint16_t port;
    };

    std::vector<NodeInfo> GetNodeInfo();
    std::vector<NodeInfo> GetTestNodeInfo();
}