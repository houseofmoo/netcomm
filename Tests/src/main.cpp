#include <iostream>
#include <cstring>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>

#include <eROIL/print.h>
#include <eROIL/eroil.h>
#include "tests.h"
#include "labels.h"
#include "scenario/scenario.h"
#include "randomizer/randomizer.h"

void generate_specific_scenario(int seed) {
    auto scenario = generate_test_scenario(seed);
    if (scenario.nodes.empty()) {
        PRINT("scenarios empty");
    }
    write_scenario_to_file(scenario);
}

void generate_and_write_scenarios(int num_scenarios) {
    // generate a visualization of N scenarios
    for (int i = 0; i < num_scenarios; i++) {
        auto seed = rng::random_int(1000, 9999);
        PRINT("generating scenarios for seed: ", seed);
        auto scenario = generate_test_scenario(seed);
        if (scenario.nodes.empty()) {
            PRINT("scenarios empty");
        }
        write_scenario_to_file(scenario);
    }
}

int run_network(int id, int seed) {
    auto scenario = generate_test_scenario(seed);
    if (scenario.nodes.size() < static_cast<size_t>(id) || scenario.nodes[id].id != id) {
        ERR_PRINT("scenarios list did not contain this nodes labels list");
        return 1;
    }

    if (scenario.nodes[id].send_labels.size() <= 0 && scenario.nodes[id].recv_labels.size() <= 0) {
        ERR_PRINT("scenarios list did not have any send/recv labels");
        return 1;
    }

    auto recv_labels = make_recv_list(scenario.nodes[id].recv_labels);
    auto send_labels = make_send_list(scenario.nodes[id].send_labels);

    bool success = init_manager(id);
    if (!success) {
        ERR_PRINT("manager init failed");
        return 1;
    }

    // open recvs
    for (auto& r : recv_labels) {
        std::thread t([r]() {
            do_recv(std::move(r));
        });
        t.detach();
    }

    // open send
    for (size_t i = 0; i < send_labels.size() - 1; i++) {
        auto ptr = send_labels[i];
        std::thread t([ptr]() {
            do_send(std::move(ptr));
        });
        t.detach();
    }

    auto ptr = send_labels[send_labels.size() - 1];
    do_send(std::move(ptr));
    return 0;
}

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }
    (void)id;

    //generate_specific_scenario(3888);
    //generate_and_write_scenarios(20);
    run_network(id, 3888);

   
    return 0;
}