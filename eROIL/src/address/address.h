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

    void insert_addresses(NodeInfo self, std::vector<NodeInfo> nodes);
    NodeAddress get_address(NodeId id);
    const std::unordered_map<NodeId, NodeAddress>& get_address_book();

    // testing options
    void set_all_local();
    void set_all_remote();
}