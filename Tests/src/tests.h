#pragma once

#include <iostream>
#include <cstring>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>

#include <eROIL/print.h>
#include <eROIL/eroil_cpp.h>
#include "io.h"
#include "labels.h"
#include "scenario/scenario.h"
#include "randomizer/randomizer.h"

inline int timed_test(int id) {

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

inline void generate_specific_scenario(const int seed, int num_nodes, const bool detailed) {
    PRINT("generating scenario for seed: ", seed);
    auto scenario = generate_test_scenario(seed, num_nodes);
    if (scenario.nodes.empty()) {
        PRINT("scenarios empty");
    }
    PRINT("writing scenario_", seed, " to file");
    write_scenario_to_file(scenario, detailed);
}

inline void generate_and_write_scenarios(const int num_scenarios, int num_nodes, const bool detailed) {
    // generate a visualization of N scenarios
    for (int i = 0; i < num_scenarios; i++) {
        auto seed = rng::random_int(1000, 9999);
        PRINT("generating scenario for seed: ", seed);
        auto scenario = generate_test_scenario(seed, num_nodes);
        if (scenario.nodes.empty()) {
            PRINT("scenarios empty");
        }
        write_scenario_to_file(scenario, detailed);
    }
}

// simulate a full network with multiple sending/recving labels
inline int run_network_sim(int id, int seed, int num_nodes, bool show_send, bool show_recv) {
    auto scenario = generate_test_scenario(seed, num_nodes);
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

    LOG("TEST.exe: spawning recv threads: ", recv_labels.size());
    LOG("TEST.exe: spawning send threads: ", send_labels.size() - 1);

    // open recvs
    for (auto& r : recv_labels) {
        std::thread t([r, show_recv]() {
            do_recv(std::move(r), show_recv);
        });
        t.detach();
    }

    // open send
    for (size_t i = 0; i < send_labels.size() - 1; i++) {
        auto ptr = send_labels[i];
        std::thread t([ptr, show_send]() {
            do_send(std::move(ptr), show_send);
        });
        t.detach();
    }

    auto ptr = send_labels[send_labels.size() - 1];
    do_send(std::move(ptr), show_send);
    return 0;
}

// add and remove labels test
inline int add_remove_labels_test(int id) {
    (void)id;
    // bool success = init_manager(id);
    // if (!success) {
    //     ERR_PRINT("manager init failed");
    //     return 1;
    // }

    // if (id == 0) {
    //     auto label = std::make_shared<SendLabel>(make_send_label(0, 1024, 1000));
    //     PRINT("opening send label for: ", label->id);
    //     auto handle = open_send(label->id, label->get_buf(), label->size);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));

    //     int count = 0;
    //     while (count < 15) {
    //         count += 1;

    //         PRINT("SEND [", label->id, "] val: ", count);

    //         std::memcpy(label->get_buf(), &count, sizeof(count));
    //         send_label(handle, label->get_buf(), label->size, 0);
    //         std::this_thread::sleep_for(std::chrono::milliseconds(label->sleep_time));
    //     }
        
    //     PRINT("closing send label for: ", label->id, " for 10 seconds");
    //     close_send(handle);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10 * 1000));

    //     PRINT("opening send label for: ", label->id);
    //     auto new_handle = open_send(label->id, label->get_buf(), label->size);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

    //     count = 0;
    //     while (count < 30) {
    //         count += 1;

    //         PRINT("SEND [", label->id, "] val: ", count);

    //         std::memcpy(label->get_buf(), &count, sizeof(count));
    //         send_label(new_handle, label->get_buf(), label->size, 0);
    //         std::this_thread::sleep_for(std::chrono::milliseconds(label->sleep_time));
    //     }
    // } else {
    //     auto label = std::make_shared<RecvLabel>(make_recv_label(0, 1024));
        
    //     PRINT("opening recv label for: ", label->id);
    //     open_recv(label->id, label->get_buf(), label->size, label->sem);
        
    //     int count = 0;
    //     int prev_count = 0;
    //     bool first_recv = true;
    //     while (true) {
    //         label->wait();
    //         std::memcpy(&count, label->get_buf(), sizeof(count));

    //         if (prev_count + 1 != count) {
    //             if (first_recv) {
    //                 first_recv = false;
    //             } else {
    //                 ERR_PRINT("RECV [", label->id, "] got count out of order expected=", prev_count + 1, " got=", count);
    //             }
    //         } else {
    //             LOG("RECV [", label->id, "] got: ", count);
    //         }
    //         prev_count = count;
    //     }
    // }
    return 0;
}

inline void small_test(int id) {
    bool success = init_manager(id);
    LOG("manager init success: ", success);

    if (id == 0) {
        auto recv = std::make_shared<RecvLabel>(make_recv_label(0, 1024));
        auto recv_handle = open_recv_label(recv->id, recv->buf.get(), recv->size, 1, nullptr, recv->sem, nullptr, 0, 2);

        // auto send = std::make_shared<SendLabel>(make_send_label(0, 1024, 1000));
        // auto send_handle = open_send_label(send->id, send->buf.get(), send->size, 1, nullptr, nullptr, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        int count = 0;
        while (true) {
            // send_label(send_handle, nullptr, 0, 0, 0);
            // LOG("sent: ", count);
            // count += 1;
            // std::memcpy(send->buf.get(), &count, sizeof(count));

            recv->wait();
            std::memcpy(&count, recv->buf.get(), sizeof(count));
            LOG("recvd: ", count);
            recv_dismiss(recv_handle, 1);
        }
        close_recv_label(recv_handle);
       //close_send_label(send_handle);
        LOG("exit");
        
    } else {
        auto send = std::make_shared<SendLabel>(make_send_label(0, 1024, 1000));
        auto handle = open_send_label(send->id, send->buf.get(), send->size, 1, nullptr, nullptr, 0);
        int count = 1;
        //if (id == 2) count = 1001;
        while (true) {

            send_label(handle, nullptr, 0, 0, 0);
            LOG("sent: ", count);
            count += 1;
            std::memcpy(send->buf.get(), &count, sizeof(count));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        close_send_label(handle);
        LOG("exit");
    }
}