#include "scenario/scenario.h"
#include "randomizer/randomizer.h"
#include <random>
#include <fstream>

int align_64_bit(int num) {
    return (num + 63) & ~63;;
}

std::vector<NodeScenario> generate_test_scenario() {
    std::vector<NodeScenario> scenarios(NUM_NODES);
    std::mt19937 rng(12345); // seed randomizer

    // genreate labels 0..199 and a random 64 bit aligned size
    std::vector<ScenarioLabel> labels;
    for (int i = 0; i < MAX_LABELS; i++) {
        int size = align_64_bit(seed::random_int(KILOBYTE, 500*KILOBYTE, rng));
        labels.push_back(ScenarioLabel{
            i, 
            static_cast<size_t>(size),
            false
        });
    }

    for (int i = 0; i < NUM_NODES; i++) {
        scenarios[i].id = i;

        // randomize number of labels to send
        int num_to_send = seed::random_int(1, MAX_SEND_LABELS, rng);
        auto send_label_indices = seed::random_unique_range(0, MAX_LABELS, num_to_send, rng);

        // pick send labels that are not already claimed
        for (const auto& lbl_idx : send_label_indices) {
            if (labels[lbl_idx].is_sent) continue; // someone already sending this

            int wait_ms = seed::random_int(50, 500, rng);
            scenarios[i].send_labels.push_back(
                ScenarioSendLabel{
                    labels[lbl_idx].id,
                    labels[lbl_idx].size,
                    wait_ms
                }
            );
            labels[lbl_idx].is_sent = true;
        }

        // randomize number of labels to recv
        int num_to_recv = seed::random_int(1, MAX_RECV_LABELS, rng);
        auto recv_label_indices = seed::random_unique_range(0, MAX_LABELS, num_to_recv, rng);

        // pick recv labels that are being sent
        for (const auto& lbl_idx : recv_label_indices) {
            if (!labels[lbl_idx].is_sent) continue; // no one sends this, skip it

            scenarios[i].recv_labels.push_back(
                ScenarioRecvLabel{
                    labels[lbl_idx].id,
                    labels[lbl_idx].size,
                }
            );
        }
    }

    return scenarios;
}


void write_to_file(const char* filepath, const std::vector<NodeScenario>& scenarios) {
    if (scenarios.empty()) return;

    std::ofstream file(filepath);
    file << "SCENARIOS:" << std::endl;
    for (const auto& s : scenarios) {
        file << "  ID: " << s.id << std::endl;

        file << "    send labels:";
        for (const auto& send : s.send_labels) {
            file << "[" << send.id << ", " << send.size << ", " << send.send_rate_ms << "], ";
        }
        file << std::endl;

        file << "    recv labels:";
        for (const auto& recv : s.recv_labels) {
            file << "[" << recv.id << ", " << recv.size << "], ";
        }
        file << std::endl;
        file << std::endl;
    }
}