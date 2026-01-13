#include "address.h"
#include <eROIL/print.h>

namespace eroil::addr {
    static std::unordered_map<NodeId, NodeAddress> address_book;

    const std::unordered_map<NodeId, NodeAddress>& get_address_book() {
        return address_book;
    }

    NodeAddress get_address(NodeId id) {
        auto it = address_book.find(id);
        if (it == address_book.end()) {
            return NodeAddress{ RouteKind::None, id, std::string(), 0 };
        }

        return it->second;
    }

    void insert_addresses(NodeInfo self, std::vector<NodeInfo> nodes) {
        // we'll insert oursevels as a node, but thats fine 
        for (const auto& node : nodes) {
            RouteKind kind = RouteKind::None;
            if (node.id == self.id) {
                kind = RouteKind::Self;
            } else if (node.ip == self.ip) {
                kind = RouteKind::Shm;
            } else {
                kind = RouteKind::Socket;
            }
            auto [it, inserted] = address_book.try_emplace(
                node.id,
                NodeAddress { kind, node.id, node.ip, node.port }
            );

            if (!inserted) {
                ERR_PRINT("Address book attempted to add duplicate NodeAddress");
            }
        }
    }
    
    NodeAddress find_node_id(std::string ip, uint16_t port) {
        for (auto [id, addr] : address_book) {
            if (addr.ip == ip && addr.port == port) return addr;
        }

        return NodeAddress{ RouteKind::None, INVALID_NODE, ip, port };
    }

    void set_all_local() {
        for (auto& [id, info] : address_book) {
            if (info.kind == RouteKind::Self) continue;
            info.kind = RouteKind::Shm;
        }
    }

    void set_all_remote() {
        for (auto& [id, info] : address_book) {
            if (info.kind == RouteKind::Self) continue;
            info.kind = RouteKind::Socket;
        }
    }

    void split_by_id(int my_id) {
        for (auto& [id, info] : address_book) {
            if (info.kind != RouteKind::Self) {
                if (my_id % 2 == 0) {
                    if (id % 2 == 0) { 
                        info.kind = RouteKind::Shm;
                    } else {
                        info.kind = RouteKind::Socket;
                    }
                } else {
                    if (id % 2 == 0) { 
                        info.kind = RouteKind::Socket;
                    } else {
                        info.kind = RouteKind::Shm;
                    }
                }                
            }
        }
    }
}