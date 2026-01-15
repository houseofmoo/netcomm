#include <iostream>
#include <cstring>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>

#include <eROIL/print.h>
#include <eROIL/eroil.h>
#include "io.h"
#include "labels.h"
#include "scenario/scenario.h"
#include "randomizer/randomizer.h"

int timed_test(int id) {

    // SOCKET:
    // got an average send/recv latency of 
    // 460199 ns = 460.2 microsec = 0.4602 millisec
    // with a data size of 1 KB

    // SHM
    // got an average send/recv latency of
    // 471680 ns = 471.7 microsec = 0.4717 millisec

    // these are similar, that the real diff is likely the print time?
    // maybe store the values then do a delta on them
    // so instead of sending a counter, it sends the ns time

    bool success = init_manager(id);
    if (!success) {
        ERR_PRINT("manager init failed");
        return 1;
    }

    // expect to only have id 0 and id 1 running
    if (id == 0) {
        auto ptr = std::make_shared<SendLabel>(make_send_label(0, KILOBYTE, 100));
        do_timed_send(std::move(ptr));
    }

    if (id == 1) {
        auto ptr = std::make_shared<RecvLabel>(make_recv_label(0, KILOBYTE));
        do_timed_recv(std::move(ptr));
    }

    return 0;
}

void generate_specific_scenario(const int seed, const bool detailed) {
    PRINT("generating scenario for seed: ", seed);
    auto scenario = generate_test_scenario(seed);
    if (scenario.nodes.empty()) {
        PRINT("scenarios empty");
    }
    PRINT("writing scenario_", seed, " to file");
    write_scenario_to_file(scenario, detailed);
}

void generate_and_write_scenarios(const int num_scenarios, const bool detailed) {
    // generate a visualization of N scenarios
    for (int i = 0; i < num_scenarios; i++) {
        auto seed = rng::random_int(1000, 9999);
        PRINT("generating scenario for seed: ", seed);
        auto scenario = generate_test_scenario(seed);
        if (scenario.nodes.empty()) {
            PRINT("scenarios empty");
        }
        write_scenario_to_file(scenario, detailed);
    }
}

int run_network_sim(int id, int seed) {
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
            do_recv(std::move(r), false);
        });
        t.detach();
    }

    // open send
    for (size_t i = 0; i < send_labels.size() - 1; i++) {
        auto ptr = send_labels[i];
        std::thread t([ptr]() {
            do_send(std::move(ptr), false);
        });
        t.detach();
    }

    auto ptr = send_labels[send_labels.size() - 1];
    do_send(std::move(ptr), false);
    return 0;
}

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }
    (void)id;

    //generate_specific_scenario(3888, false);
    //generate_and_write_scenarios(20, false);
    //run_network_sim(id, 3888);
    timed_test(id);

   
    return 0;
}