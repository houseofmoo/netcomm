#include <iostream>
#include <cstring>
#include <memory>
#include <eROIL/print.h>
#include <eROIL/eroil.h>

#include <thread>
#include <chrono>

#include "windows_hdr.h"
#include "labels.h"

void send(int label_id, int label_size, int sleep_time) {
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

void recv(int label_id, size_t label_size) {
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

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }

    init_manager(id);

    switch (id) {
        case 0: {
            std::thread send0([]() {
                send(0, 10 * KILOBYTE, 3000);
            });
            send0.detach();

            send(1, 5 * KILOBYTE, 1000);
            break;
        }
        default: {
            std::thread recv0([]() {
                recv(0, 10 * KILOBYTE);
            });
            recv0.detach();

            recv(1, 5 * KILOBYTE);
            break;
        }
    }

    
    return 0;
}