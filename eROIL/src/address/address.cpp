#include "address.h"
#include <eROIL/print.h>

namespace eroil {
    void Address::insert_addresses(NodeInfo self, std::vector<NodeInfo> nodes) {
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
            auto [it, inserted] = addresses.try_emplace(
                node.id,
                NodeAddress { kind, node.id, node.ip, node.port }
            );

            if (!inserted) {
                ERR_PRINT("Address book attempted to add duplicate NodeAddress");
            }
        }
    }

    NodeAddress Address::get(NodeId id) {
        auto it = addresses.find(id);
        if (it == addresses.end()) {
            return NodeAddress{ RouteKind::None, 0, std::string(), 0 };
        }

        return it->second;
    }
}