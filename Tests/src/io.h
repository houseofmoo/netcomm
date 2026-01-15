#pragma once
#include <thread>
#include <chrono>
#include <memory>
#include <ctime>

#include <eROIL/eroil.h>
#include <eROIL/print.h>
#include "labels.h"


inline void do_send(std::shared_ptr<SendLabel> label, const bool show_send) {
    PRINT("opening send label for: ", label->id);
    auto handle = open_send(label->id, label->get_buf(), label->size);

    int count = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    while (true) {
        count += 1;

        if (show_send) {
            PRINT("SEND [", label->id, "] val: ", count);
        }

        std::memcpy(label->get_buf(), &count, sizeof(count));
        send_label(handle, label->get_buf(), label->size, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(label->sleep_time));
    }
}

inline void do_timed_send(std::shared_ptr<SendLabel> label) {
    PRINT("opening send label for: ", label->id);
    auto handle = open_send(label->id, label->get_buf(), label->size);

    int count = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    while (true) {
        count += 1;
        std::memcpy(label->get_buf(), &count, sizeof(count));

        now = std::chrono::steady_clock::now();
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

        PRINT(ns, " SEND [", label->id, "] val: ", count);
        send_label(handle, label->get_buf(), label->size, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(label->sleep_time));
    }
}

inline void do_recv(std::shared_ptr<RecvLabel> label, const bool show_recvd) {
    PRINT("opening recv label for: ", label->id);
    open_recv(label->id, label->get_buf(), label->size, label->sem);
    int count = 0;
    int prev_count = 0;
    bool first_recv = true;

    while (true) {
        label->wait();
        std::memcpy(&count, label->get_buf(), sizeof(count));

        if (prev_count + 1 != count) {
            if (first_recv) {
                first_recv = false;
            } else {
                ERR_PRINT("RECV [", label->id, "] got count out of order expected=", prev_count + 1, " got=", count);
            }
        } else if (show_recvd) {
            PRINT("RECV [", label->id, "] got: ", count);
        }
        prev_count = count;
    }
}

inline void do_timed_recv(std::shared_ptr<RecvLabel> label) {
    PRINT("opening recv label for: ", label->id);
    open_recv(label->id, label->get_buf(), label->size, label->sem);
    int count = 0;
    int prev_count = 0;
    bool first_recv = true;

    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    while (true) {
        label->wait();

        now = std::chrono::steady_clock::now();
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

        std::memcpy(&count, label->get_buf(), sizeof(count));
        if (prev_count + 1 != count) {
            if (first_recv) {
                first_recv = false;
            } else {
                ERR_PRINT(ns, " RECV [", label->id, "] got count out of order expected=", prev_count + 1, " got=", count);
            }
        } else {
            PRINT(ns, " RECV [", label->id, "] got: ", count);
        }
        prev_count = count;
    }
}