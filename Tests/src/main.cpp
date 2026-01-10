#include <iostream>
#include <cstring>
#include <memory>
#include <eROIL/print.h>
#include <eROIL/eroil.h>

#include <thread>
#include <chrono>

#include <windows.h>

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }

    std::cout << "init_manager(" << id << ")" << std::endl;
    init_manager(id);
    
    auto buf = std::make_unique<uint8_t[]>(1024);

    if (id % 2 == 0) {
        auto handle = open_send(5, buf.get(), 1024);
        int count = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        while (true) {
            //PRINT("sending ", count);
            std::memcpy(buf.get(), &count, sizeof(count));
            send_label(handle, buf.get(), 1024, 0);
            count += 1;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

    } else {
        auto sem = ::CreateSemaphoreW(nullptr, 0, 1000, nullptr);
        PRINT("sem handle: ", sem);
        open_recv(5, buf.get(), 1024, sem);
        int count = 0;
        int prev_count = 0;
        while (true) {
            ::WaitForSingleObject(sem, INFINITE);
            std::memcpy(&count, buf.get(), sizeof(count));
            //PRINT("recvd :", count);
            if (prev_count + 1 != count) {
                PRINT("got count out of order expected=", prev_count + 1, " got=", count);
            }
            prev_count = count;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 2000));
    std::cout << "sleep ends" << std::endl;
    return 0;
}