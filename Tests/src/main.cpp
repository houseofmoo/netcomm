#include <iostream>
#include <cstring>
#include <memory>
#include <eROIL/print.h>
#include <eROIL/eroil.h>

#include <thread>
#include <chrono>

#include "windows_hdr.h"
#include "tests.h"
#include "labels.h"

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }

    init_manager(id);

    switch (id) {
        case 0: {
            std::thread t([]() {
                do_send(0, 1*KILOBYTE, 1000);
            });
            t.detach();

            do_recv(1, 1*KILOBYTE);
            break;
        }
        default: {
            std::thread t([](){
                do_recv(0, 1*KILOBYTE);
            });
            t.detach();

            do_send(1, 1*KILOBYTE, 1000);
            break;
        }
    }

    
    return 0;
}