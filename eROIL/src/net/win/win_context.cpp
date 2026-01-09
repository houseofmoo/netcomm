#include <winsock2.h>
#include <ws2tcpip.h>
#include "net/socket.h"

namespace eroil::net {
    NetContext::NetContext() {
        WSADATA wsa{};
        m_ok = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }

    NetContext::~NetContext() {
        if (m_ok) {
            WSACleanup();
        }
    }

    NetContext::NetContext(NetContext&& other) noexcept
        : m_ok(other.m_ok) {
        other.m_ok = false;
    }

    NetContext& NetContext::operator=(NetContext&& other) noexcept {
        if (this != &other) {
            if (m_ok) {
                WSACleanup();
            }
            m_ok = other.m_ok;
            other.m_ok = false;
        }
        return *this;
    }

    bool NetContext::ok() const noexcept {
        return m_ok;
    }
}