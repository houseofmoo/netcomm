#include "peer_state.h"
#include <algorithm>
#include "assertion.h"

namespace eroil {
    static LabelDeltas diff_sorted(
        const std::array<LabelInfo, MAX_LABELS>& prev, 
        const std::array<LabelInfo, MAX_LABELS>& curr) {

        DB_ASSERT(std::is_sorted(prev.begin(), prev.end()), "prev labels not sorted");
        DB_ASSERT(std::is_sorted(curr.begin(), curr.end()), "curr labels not sorted");

        LabelDeltas delta;
        delta.added.reserve(32);
        delta.removed.reserve(32);

        size_t prev_index = 0;
        size_t curr_index = 0;

        while (prev_index < prev.size() && curr_index < curr.size()) {
            const Label prev_label = prev[prev_index].label;
            const Label curr_label = curr[curr_index].label;

            if (prev_label <= INVALID_LABEL) {
                prev_index += 1;
                continue;
            }

            if (curr_label <= INVALID_LABEL) {
                curr_index += 1;
                continue;
            }

            if (prev_label < curr_label) {
                delta.removed.push_back(prev[prev_index]);
                prev_index += 1;
            } else if (curr_label < prev_label) {
                delta.added.push_back(curr[curr_index]);
                curr_index += 1;
            } else {
                // same label
                prev_index += 1;
                curr_index += 1;
            }
        }

        // insert any remaining indices
        while (prev_index < prev.size()) {
            if (prev[prev_index].label > INVALID_LABEL) {
                delta.removed.push_back(prev[prev_index]);
            }
            prev_index += 1;
        }

        while (curr_index < curr.size()) {
            if (curr[curr_index].label > INVALID_LABEL) {
                delta.added.push_back(curr[curr_index]);
            }
            curr_index += 1;
        }

        return delta;
    }

    LabelDeltas PeerState::update_send(const BroadcastMessage& msg) {
        auto it = m_peer_send_labels.find(msg.id);
        if (it == m_peer_send_labels.end()) {
            m_peer_send_labels.emplace(msg.id, PeerLabels{
                msg.send_labels.gen,
                msg.send_labels.labels
            });

            LabelDeltas delta;
            delta.added.reserve(MAX_LABELS);

            for (const auto& send : msg.send_labels.labels) {
                if (send.label > INVALID_LABEL) {
                    delta.added.push_back(send);
                }
            }
            return delta;
        }

        auto& prev_state = it->second;

        // no change
        if (prev_state.gen == msg.send_labels.gen) {
            return {};
        }

        // get delta
        LabelDeltas delta = diff_sorted(prev_state.labels, msg.send_labels.labels);

        // update to capture new state
        prev_state.gen = msg.send_labels.gen;
        prev_state.labels = msg.send_labels.labels;

        return delta;
    }
    
    LabelDeltas PeerState::update_recv(const BroadcastMessage& msg) {
        auto it = m_peer_recv_labels.find(msg.id);
        if (it == m_peer_recv_labels.end()) {
            m_peer_recv_labels.emplace(msg.id, PeerLabels{
                msg.recv_labels.gen,
                msg.recv_labels.labels
            });

            LabelDeltas delta;
            delta.added.reserve(MAX_LABELS);

            for (const auto& recv : msg.recv_labels.labels) {
                if (recv.label > INVALID_LABEL) {
                    delta.added.push_back(recv);
                }
            }
            return delta;
        }

        auto& prev_state = it->second;
       
        // no change
        if (prev_state.gen == msg.recv_labels.gen) {
            return {};
        }

        // get delta
        LabelDeltas delta = diff_sorted(prev_state.labels, msg.recv_labels.labels);

        // udpate to capture new state
        prev_state.gen = msg.recv_labels.gen;
        prev_state.labels = msg.recv_labels.labels;

        return delta;
    }  
}