#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "config/config.h"
#include "types/types.h"

namespace eroil::addr {
    enum class RouteKind {
        None,
        Shm,
        Socket,
        Self,
    };

    struct NodeAddress {
        RouteKind kind;
        NodeId id;
        std::string ip;
        uint16_t port;
    };

    const std::unordered_map<NodeId, NodeAddress>& get_address_book();
    NodeAddress get_address(NodeId id);
    bool insert_addresses(const cfg::NodeInfo self, const std::vector<cfg::NodeInfo> nodes, const cfg::ManagerMode mode);
    NodeAddress find_node_id(std::string ip, uint16_t port);
}