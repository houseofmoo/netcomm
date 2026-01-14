#include <iostream>
#include <cstring>
#include <memory>
#include <random>
#include <thread>
#include <chrono>
#include <fstream>

#include <eROIL/print.h>
#include <eROIL/eroil.h>
// #include "tests.h"
// #include "labels.h"
#include "scenario/scenario.h"

struct Labels {
    int id;
    size_t size;
    bool is_sent;
};

struct SendLabels {
    int id;
    size_t size;
    int send_rate_ms;
};

struct RecvLabels {
    int id;
    size_t size;
};

struct NodeTestScenario {
    int id;
    std::vector<SendLabels> send_labels;
    std::vector<RecvLabels> recv_labels;
};

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }
    id = id;
    //init_manager(id);
    PRINT("generating scenarios");
    auto scenarios = generate_test_scenario();
    if (scenarios.empty()) {
        PRINT("scenarios empty");
    }
    write_to_file("output.txt", scenarios);

    // switch (id) {
    //     case 0: {
    //         std::thread t([]() {
    //             do_send(0, 1*KILOBYTE, 1000);
    //         });
    //         t.detach();

    //         do_recv(1, 1*KILOBYTE);
    //         break;
    //     }
    //     default: {
    //         std::thread t([](){
    //             do_recv(0, 1*KILOBYTE);
    //         });
    //         t.detach();

    //         do_send(1, 1*KILOBYTE, 1000);
    //         break;
    //     }
    // }
    return 0;
}

/*
    expect hard number of 20 nodes comminucating
    10 over shm
    10 over socket

    we need an ID for ourselves (from main arg)
    we need a list of labels we want to send
        label ID
        label size
        send rate

    we need a list of labels we want to recv
        label ID
        label size
        sem

    tricky part:
        all nodes should agree on this set up, which means we should pre-generate it and
        have our test functions read it in
        we also want to make sure that we dont have any nodes sending something that isnt being recv
        or nodes recving something that is not being sent

    
*/