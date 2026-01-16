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

    void insert_normal(const cfg::NodeInfo self, const std::vector<cfg::NodeInfo> nodes) {
        for (const auto& node : nodes) {
            if (node.id < 0) continue; // expected when node IDs are not sequential
            
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
                ERR_PRINT("address book attempted to add duplicate NodeAddress");
            }
        }
    }

    void insert_as_shm(const cfg::NodeInfo self, const std::vector<cfg::NodeInfo> nodes) {
        PRINT("making shm only address book");
        for (const auto& node : nodes) {
            if (node.id < 0) continue; // expected when node IDs are not sequential

            RouteKind kind = RouteKind::None;
            if (node.id == self.id) {
                kind = RouteKind::Self;
            } else {
                kind = RouteKind::Shm;
            }

            auto [it, inserted] = address_book.try_emplace(
                node.id,
                NodeAddress { kind, node.id, node.ip, node.port }
            );

            if (!inserted) {
                ERR_PRINT("address book attempted to add duplicate NodeAddress");
            }
        }
    }

    void insert_as_socket(const cfg::NodeInfo self, const std::vector<cfg::NodeInfo> nodes) {
        PRINT("making socket only address book");
        for (const auto& node : nodes) {
            if (node.id < 0) continue; // expected when node IDs are not sequential

            RouteKind kind = RouteKind::None;
            if (node.id == self.id) {
                kind = RouteKind::Self;
            } else {
                kind = RouteKind::Socket;
            }

            auto [it, inserted] = address_book.try_emplace(
                node.id,
                NodeAddress { kind, node.id, node.ip, node.port }
            );

            if (!inserted) {
                ERR_PRINT("address book attempted to add duplicate NodeAddress");
            }
        }
    }

    void make_test_network(const cfg::NodeInfo self, const std::vector<cfg::NodeInfo> nodes) {
        PRINT("making test network address book");

        for (const auto& node : nodes) {
            if (node.id < 0) continue; // expected when node IDs are not sequential

            auto info = NodeAddress { RouteKind::None, node.id, node.ip, node.port };

            if (node.id == self.id) {
                info.kind = RouteKind::Self;
            } else {
                if (self.id % 2 == 0) {
                    if (info.id % 2 == 0) info.kind = RouteKind::Shm;
                    else info.kind = RouteKind::Socket;
                } else {
                    if (info.id % 2 == 0) info.kind = RouteKind::Socket;
                    else info.kind = RouteKind::Shm;
                }
            }

            auto [it, inserted] = address_book.try_emplace(node.id, info);
            if (!inserted) {
                ERR_PRINT("address book attempted to add duplicate NodeAddress");
            }
        }
    }

    bool insert_addresses(const cfg::NodeInfo self, const std::vector<cfg::NodeInfo> nodes, const cfg::ManagerMode mode) {
        if (self.id < 0) {
            ERR_PRINT("address book recvd a self.id < 0, which makes no sense");
            return false;
        }

        if (nodes.empty()) {
            ERR_PRINT("address book recvd empty NodeInfo list");
            return false;
        }

        switch (mode) {
            case cfg::ManagerMode::TestMode_Local_ShmOnly: insert_as_shm(self, nodes); break;
            case cfg::ManagerMode::TestMode_Lopcal_SocketOnly: insert_as_socket(self, nodes); break;
            case cfg::ManagerMode::TestMode_Sim_Network: make_test_network(self, nodes); break;
            
            case cfg::ManagerMode::Normal: // fall through
            default: insert_normal(self, nodes); break;
        }

        if (address_book.empty()) {
            ERR_PRINT("address book is empty after calling insert_addresses()");
            return false;
        }

        return true;
    }
    
    NodeAddress find_node_id(std::string ip, uint16_t port) {
        for (auto [id, addr] : address_book) {
            if (addr.ip == ip && addr.port == port) return addr;
        }
        return NodeAddress{ RouteKind::None, INVALID_NODE, ip, port };
    }
}