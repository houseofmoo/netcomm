#pragma once
#include <string>
#include <unordered_map>
#include "config/config.h"
#include "types/types.h"

namespace eroil {
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

    class Address {
        private:
            std::unordered_map<NodeId, NodeAddress> m_node_addresses;

        public:
            Address() = default;
            ~Address() = default;

            void insert_addresses(NodeInfo self, std::vector<NodeInfo> nodes);
            NodeAddress get(NodeId id);
    };
}