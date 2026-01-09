#include "broadcast.h"
#include <thread>
#include <chrono>
#include <memory>

namespace eroil {
    Broadcast::Broadcast() 
        : m_udp{}, 
          m_ip{"239.255.0.1"},
          m_port{0},
          m_ok{false} {}

    Broadcast::~Broadcast() {
        eroil::net::udpm::close(m_udp);
    }

    void Broadcast::setup(uint16_t port) {
        m_port = port;
        m_udp = eroil::net::udpm::open_udp_socket();
        eroil::net::udpm::bind(m_udp, "0.0.0.0", m_port);
        net::udpm::join_multicast(m_udp, "239.255.0.1");
        m_ok = true;
    }
 
    void Broadcast::send(const BroadcastMessage& msg) {
        std::lock_guard lock(m_mtx);
        if (m_ok) {
            eroil::net::udpm::send_to(m_udp, &msg, sizeof(msg), m_ip, m_port);
        }
    }

    void Broadcast::recv(BroadcastMessage& buf) const {
        net::UdpEndpoint from{};
        if (m_ok) {
            eroil::net::udpm::recv_from(m_udp, &buf, sizeof(buf), &from);
        }
    }
}