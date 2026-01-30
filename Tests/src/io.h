#pragma once
#include <thread>
#include <chrono>
#include <memory>
#include <ctime>

#include <eROIL/eroil_cpp.h>
#include <eROIL/print.h>
#include "labels.h"


inline void do_send(std::shared_ptr<SendLabel> label, const bool show_send) {
    PRINT("opening send label for: ", label->id);
    auto handle = open_send_label(label->id, label->buf.get(), label->size, 1, nullptr, nullptr, 0);

    int count = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    while (true) {
        count += 1;

        if (show_send) {
            LOG("SEND [", label->id, "] val: ", count);
        }

        std::memcpy(label->buf.get(), &count, sizeof(count));
        send_label(handle, nullptr, 0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(label->sleep_time));
    }
    close_send_label(handle);
}

inline void do_timed_send(std::shared_ptr<SendLabel> label) {
    PRINT("opening send label for: ", label->id);
    auto handle = open_send_label(label->id, label->buf.get(), label->size, 1, nullptr, nullptr, 0);

    int count = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    while (true) {
        count += 1;
        std::memcpy(label->buf.get(), &count, sizeof(count));

        now = std::chrono::steady_clock::now();
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

        LOG(ns, " SEND [", label->id, "] val: ", count);
        send_label(handle, nullptr, 0, 0, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(label->sleep_time));
    }
    close_send_label(handle);
}

inline void do_recv(std::shared_ptr<RecvLabel> label, const bool show_recvd) {
    PRINT("opening recv label for: ", label->id);
    auto handle = open_recv_label(label->id, label->buf.get(), label->size, 1, nullptr, label->sem, nullptr, 0, 2);
    int count = 0;
    int prev_count = 0;
    bool first_recv = true;

    while (true) {
        label->wait();
        std::memcpy(&count, label->buf.get(), sizeof(count));

        if (prev_count + 1 != count) {
            if (first_recv) {
                first_recv = false;
            } else {
                ERR_PRINT("RECV [", label->id, "] got count out of order expected=", prev_count + 1, " got=", count);
            }
        } else if (show_recvd) {
            LOG("RECV [", label->id, "] got: ", count);
        }
        prev_count = count;
        recv_dismiss(handle, 1);
    }
    close_recv_label(handle);
}

inline void do_timed_recv(std::shared_ptr<RecvLabel> label) {
    PRINT("opening recv label for: ", label->id);
    auto handle = open_recv_label(label->id, label->buf.get(), label->size, 1, nullptr, label->sem, nullptr, 0, 2);
    int count = 0;
    int prev_count = 0;
    bool first_recv = true;

    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    while (true) {
        label->wait();

        now = std::chrono::steady_clock::now();
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

        std::memcpy(&count, label->buf.get(), sizeof(count));
        if (prev_count + 1 != count) {
            if (first_recv) {
                first_recv = false;
            } else {
                ERR_PRINT(ns, " RECV [", label->id, "] got count out of order expected=", prev_count + 1, " got=", count);
            }
        } else {
            LOG(ns, " RECV [", label->id, "] got: ", count);
        }
        prev_count = count;
        recv_dismiss(handle, 1);
    }
    close_recv_label(handle);
}