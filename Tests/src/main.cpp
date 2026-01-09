#include <iostream>
#include <cstring>
//#include <eROIL/socket.h>

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
//         if (r.result == IoResult::Ok) {
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
//         if (r.result == IoResult::Ok) {
//             buf[r.bytes] = 0;
//             std::cout << "3 got: " << buf << "\n";
//         } else {
//             std::cout << "3 recv err=" << r.sys_error << "\n";
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// }

int main() {
    std::cout << "hello world from main.cpp2" << std::endl;

    // {
    //     eroil::net::NetContext context;
    //     if (!context.ok()) {
    //         std::cerr << "WSAStartup failed\n";
    //         return 1;
    //     }

    //     std::thread thread1([]() {
    //         RunThread1();
    //     });


    //     std::thread thread2([]() {
    //         RunThread2();
    //     });

    //     std::thread thread3([]() {
    //         RunThread3();
    //     });

    //     thread1.join();
    //     thread2.join();
    //     thread3.join();

    //     // eROIL::net::socket::TCPClient s = eROIL::net::tcp::open_tcp_socket();
    //     // eROIL::net::tcp::set_nonblocking(s, false);

    //     // if (!eROIL::net::tcp::connect(s, "127.0.0.1", 8001)) {
    //     //     std::cerr << "Connect failed\n";
    //     //     eROIL::net::tcp::close(s);
    //     //     eROIL::net::shutdown();
    //     //     return 1;
    //     // }

    //     // const char msg[] = "hello\n";
    //     // auto w = eROIL::net::tcp::send(s, msg, std::strlen(msg));
    //     // if (w.result != eROIL::net::IoResult::Ok) {
    //     //     std::cerr << "send failed, err=" << w.sys_error << "\n";
    //     // }

    //     // char buf[1024];
    //     // auto r = eROIL::net::tcp::recv(s, buf, sizeof(buf) - 1);
    //     // if (r.result == eROIL::net::IoResult::Ok) {
    //     //     buf[r.bytes] = '\0';
    //     //     std::cout << "recv: " << buf;
    //     // } else if (r.result == eROIL::net::IoResult::Closed) {
    //     //     std::cout << "peer closed\n";
    //     // } else {
    //     //     std::cerr << "recv failed/wouldblock, err=" << r.sys_error << "\n";
    //     // }

    //     // eROIL::net::tcp::close(s);
    // }
    std::cout<<"hello close"<<std::endl;
    return 0;
}