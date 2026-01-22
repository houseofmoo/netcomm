#pragma once
#include <vector>
#include <unordered_map>
#include "router/router.h"
#include "types/types.h"

namespace eroil {
    struct LabelDeltas {
        std::vector<io::LabelInfo> added;
        std::vector<io::LabelInfo> removed;
    };

    struct PeerLabels {
        std::uint64_t gen = 0;
        std::array<io::LabelInfo, MAX_LABELS> labels{};
    };

    class PeerState {
        // unused ATM, keeping around incase we need to optimize the broadcast() threads further
        private:
            std::unordered_map<NodeId, PeerLabels> m_peer_send_labels;
            std::unordered_map<NodeId, PeerLabels> m_peer_recv_labels;

        public:
            LabelDeltas update_send(const io::BroadcastMessage& msg);
            LabelDeltas update_recv(const io::BroadcastMessage& msg);
    };
}