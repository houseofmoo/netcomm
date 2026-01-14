#pragma once
#include <thread>
#include <chrono>
#include "labels.h"
#include <eROIL/eroil.h>
#include <eROIL/print.h>


inline void do_send(int label_id, int label_size, int sleep_time) {
    auto label = make_send_label(label_id, label_size);
    auto handle = open_send(label.id, label.get_buf(), label.size);

    int count = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    while (true) {
        count += 1;
        PRINT("sending: ", count);
        std::memcpy(label.get_buf(), &count, sizeof(count));
        send_label(handle, label.get_buf(), label.size, 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }
}

inline void run_n_send_labels(int num_sends) {
    // spawn N-1 threads to send, then lock calling thread in the final send loop
    int i = 0;
    for (; i < num_sends - 1; i++) {
        std::thread sender([i]() {
            do_send(i, 500*KILOBYTE, 1000);
        });
        sender.detach();
    }

    do_send(i, 500*KILOBYTE, 1000);
}

inline void do_recv(int label_id, size_t label_size) {
    auto label = make_recv_label(label_id, label_size);
    open_recv(label.id, label.get_buf(), label.size, label.sem);
    int count = 0;
    int prev_count = 0;

    while (true) {
        label.wait();
        std::memcpy(&count, label.get_buf(), sizeof(count));
        if (prev_count + 1 != count) {
            PRINT("got count out of order expected=", prev_count + 1, " got=", count);
        } else {
            PRINT("got: ", count);
        }
        prev_count = count;
    }
}

inline void run_n_recv_labels(int num_recvs) {
    int i = 0;
    for (; i < num_recvs - 1; i++) {
        std::thread recv0([i]() {
            do_recv(i, 500*KILOBYTE);
        });
        recv0.detach();
    }

    do_recv(i, 500*KILOBYTE);
}
