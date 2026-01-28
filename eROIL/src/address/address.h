#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "config/config.h"
#include "types/const_types.h"

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

    struct PeerSet {
        std::vector<addr::NodeAddress> local;
        std::vector<addr::NodeAddress> remote;
        std::vector<addr::NodeAddress> remote_connect_to;
    };

    bool init_address_book(NodeId my_id);
    const std::unordered_map<NodeId, NodeAddress>& get_address_book();
    NodeAddress get_address(NodeId id);
    PeerSet get_peer_set(NodeId my_id);
    
    // set up test-mode address book
    void all_shm_address_book();
    void all_socket_address_book();
    void test_network_address_book();
}