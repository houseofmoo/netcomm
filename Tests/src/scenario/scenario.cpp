#include "scenario/scenario.h"
#include "randomizer/randomizer.h"
#include <random>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <array>
#include <filesystem>
#include "labels.h"
#include <eROIL/print.h>

struct ScenarioLabel {
    int id;
    size_t size;
    bool is_sent;
};

int align_64_bit(int num) {
    return (num + 63) & ~63;;
}

TestScenario generate_test_scenario(int seed) {
    TestScenario scenario(seed, NUM_NODES);
    std::mt19937 rng(scenario.seed); // seed randomizer

    // genreate labels 0..MAX_LABELS and a random 64 bit aligned size
    std::array<ScenarioLabel, MAX_LABELS> labels;
    for (int i = 0; i < MAX_LABELS; i++) {
        int size = align_64_bit(seed::random_int(KILOBYTE, 500*KILOBYTE, rng));
        labels[i] = (ScenarioLabel{
            i, 
            static_cast<size_t>(size),
            false
        });
    }

    // generate everyones send labels list
    for (int i = 0; i < NUM_NODES; i++) {
        auto& node = scenario.nodes[i];
        node.id = i;

        // randomize number of labels to send
        int num_to_send = seed::random_int(MIN_SEND_LABELS, MAX_SEND_LABELS, rng);
        auto send_label_indices = seed::random_unique_range(0, MAX_LABELS - 1, num_to_send, rng);

        // pick labels to send that have not been picked by someone else
        for (const auto& lbl_idx : send_label_indices) {
            // someone else sends this, skip it
            if (labels[lbl_idx].is_sent) continue; 

            // send this label
            int wait_ms = seed::random_int(MIN_SEND_RATE, MAX_SEND_RATE, rng);
            node.send_labels.push_back(
                ScenarioSendLabel{
                    labels[lbl_idx].id,
                    labels[lbl_idx].size,
                    wait_ms
                }
            );
            labels[lbl_idx].is_sent = true;
        }
        node.send_count = node.send_labels.size();
    }

    // generate everyones recv labels list
    for (int i = 0; i < NUM_NODES; i++) {
        auto& node = scenario.nodes[i];
        node.id = i;

        // randomize number of labels to recv
        int num_to_recv = seed::random_int(MIN_RECV_LABELS, MAX_RECV_LABELS, rng);
        auto recv_label_indices = seed::random_unique_range(0, MAX_LABELS - 1, num_to_recv, rng);

        // pick labels to recv that is marked as being sent
        for (const auto& lbl_idx : recv_label_indices) {
            // no one sends this, skip it
            if (!labels[lbl_idx].is_sent) continue;
            
            // do not recv a label we send
            auto it = std::find_if(
                node.send_labels.begin(),
                node.send_labels.end(),
                [&](const ScenarioSendLabel& s){
                    return s.id == labels[lbl_idx].id;
                }
            );
            if (it != node.send_labels.end()) continue;

            // recv this label
            node.recv_labels.push_back(
                ScenarioRecvLabel{
                    labels[lbl_idx].id,
                    labels[lbl_idx].size,
                }
            );
        }
        node.recv_count = node.recv_labels.size();
    }

    return scenario;
}

void write_scenario_to_file(const TestScenario& scenario, const bool detailed) {
    if (scenario.nodes.empty()) return;
    std::string dir = "scenarios";
    std::string filepath = dir + "/scenario_" + std::to_string(scenario.seed) +  ".txt";

    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directory(dir);
    }

    std::ofstream file(filepath);

    file << "SCENARIO " << scenario.seed << std::endl;
    file << std::endl;

    for (const auto& s : scenario.nodes) {
        file << "  NODE ID: " << s.id << std::endl;

        std::string send_labels_line = "    send labels (" + std::to_string(s.send_count) + "): ";
        file << std::left << std::setw(22) << send_labels_line;

        for (size_t i = 0; i < s.send_labels.size(); i++) {
            auto slbl = s.send_labels[i];
            std::string line;
            
            if (detailed) {
                line = "[" + std::to_string(slbl.id) + ", " + std::to_string(slbl.size) + ", " + std::to_string(slbl.send_rate_ms) + "]";
                file << std::left << std::setw(20) << line;
            } else {
                line = line = "[" + std::to_string(slbl.id) + "]";
                file << std::left << std::setw(6) << line;
            }

            if (i + 1 >= s.send_labels.size()) file << std::endl;
        }
        if (s.send_labels.size() <= 0) file << std::endl;

        std::string recv_labels_line = "    recv labels (" + std::to_string(s.recv_count) + "): ";
        file << std::left << std::setw(22) << recv_labels_line;

        for (size_t i = 0; i < s.recv_labels.size(); i++) {
            auto rlbl = s.recv_labels[i];
            std::string line;

            if (detailed) {
                line = "[" + std::to_string(rlbl.id) + ", " + std::to_string(rlbl.size) + "]";
                file << std::left << std::setw(20) << line;
            } else {
                line = "[" + std::to_string(rlbl.id) + "]";
                file << std::left << std::setw(6) << line;
            }

            if (i + 1 >= s.recv_labels.size()) file  << std::endl;
        }
        file << std::endl;
        if (s.recv_labels.size() <= 0) file << std::endl;

        file.close();
    }
}