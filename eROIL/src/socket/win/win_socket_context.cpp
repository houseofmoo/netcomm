#include "socket/socket_context.h"
#include "windows_hdr.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace eroil::sock {
    SocketContext::SocketContext() {
        WSADATA wsa{};
        m_ok = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }

    SocketContext::~SocketContext() {
        if (m_ok) {
            WSACleanup();
        }
    }

    bool SocketContext::ok() const noexcept {
        return m_ok;
    }

    bool init_platform_sockets() {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }

    void cleanup_platform_sockets() {
        WSACleanup();
    }
}
