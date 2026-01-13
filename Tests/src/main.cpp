#include <iostream>
#include <cstring>
#include <memory>
#include <eROIL/print.h>
#include <eROIL/eroil.h>

#include <thread>
#include <chrono>

#include <windows.h>

void sender(int /*id*/) {
    auto buf = std::make_unique<uint8_t[]>(1024);
    auto handle = open_send(5, buf.get(), 1024);
    int count = 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    while (true) {
        PRINT("sending: ", count);
        std::memcpy(buf.get(), &count, sizeof(count));
        send_label(handle, buf.get(), 1024, 0);
        count += 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void recvr(int /*id*/) {
    auto buf = std::make_unique<uint8_t[]>(1024);
    auto sem = ::CreateSemaphoreW(nullptr, 0, 1000, nullptr);
    open_recv(5, buf.get(), 1024, sem);

    int count = 0;
    int prev_count = 0;
    while (true) {
        ::WaitForSingleObject(sem, INFINITE);
        std::memcpy(&count, buf.get(), sizeof(count));
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
        case 0: sender(id);
        case 1: recvr(id);
        default: recvr(id);
    }

    
    return 0;
}