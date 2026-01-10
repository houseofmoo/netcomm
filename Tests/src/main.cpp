#include <iostream>
#include <cstring>
#include <eROIL/print.h>
#include <eROIL/eroil.h>

#include <thread>
#include <chrono>

// void RunThread1() {
//     using namespace eroil::net;

//     socket::UDPMulti udp = udpm::open_udp_socket();
//     udpm::bind(udp, "0.0.0.0", 8080);
//     udpm::join_multicast(udp, "239.255.0.1");

//     char buf[1024];
//     UdpEndpoint from{};

//     while (true) {
//         auto r = udpm::recv_from(udp, buf, sizeof(buf)-1, &from);
//         if (r.result == SocketIoResult::Ok) {
//             buf[r.bytes] = 0;
//             std::cout << "1 got: " << buf << "\n";
//         } else {
//             std::cout << "1 recv err=" << r.sys_error << "\n";
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// }


// void RunThread2() {
//     using namespace eroil::net;

//     socket::UDPMulti udp = udpm::open_udp_socket();
//     //udpm::join_multicast(udp, "127.0.0.1");

//     const char msg[] = "hello over udp\n";
//     while (true) {
//         auto w = udpm::send_to(udp, msg, std::strlen(msg), "239.255.0.1", 8080);
//         std::cout << "send results: " << (int)w.result << std::endl;
//         std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//     }
// }

// void RunThread3() {
//     using namespace eroil::net;

//     socket::UDPMulti udp = udpm::open_udp_socket();
//     udpm::bind(udp, "0.0.0.0", 8080);
//     udpm::join_multicast(udp, "239.255.0.1");

//     char buf[1024];
//     UdpEndpoint from{};

//     while (true) {
//         auto r = udpm::recv_from(udp, buf, sizeof(buf)-1, &from);
//         if (r.result == SocketIoResult::Ok) {
//             buf[r.bytes] = 0;
//             std::cout << "3 got: " << buf << "\n";
//         } else {
//             std::cout << "3 recv err=" << r.sys_error << "\n";
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// }

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }

    std::cout << "init_manager(" << id << ")" << std::endl;
    init_manager(id);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 20));
    std::cout << "sleep ends" << std::endl;
    return 0;
}