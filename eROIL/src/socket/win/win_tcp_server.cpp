#if defined(EROIL_WIN32)
#include "socket/tcp_socket.h"
#include "windows_hdr.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace eroil::sock {
    static SOCKET as_native(socket_handle handle) noexcept {
        return static_cast<SOCKET>(handle);
    }

    static socket_handle from_native(SOCKET handle) noexcept {
        return static_cast<socket_handle>(handle);
    }

    SockResult TCPServer::bind(uint16_t port, const char* ip) {
        if (!handle_valid()) { 
            return SockResult { SockErr::InvalidHandle, SockOp::Bind, 0, 0 };
        }

        // pseudo-validate ip
        if (!ip || *ip == '\0') {
            return SockResult{ SockErr::InvalidIp, SockOp::Bind, 0, 0 };
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            return SockResult{ SockErr::InvalidIp, SockOp::Bind, 0, 0 };
        }

        BOOL reuse = TRUE;
        ::setsockopt(
            as_native(m_handle), 
            SOL_SOCKET, 
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse),
            sizeof(reuse)
        );

        if (::bind(as_native(m_handle), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            int err = ::WSAGetLastError();
            return SockResult{ map_err(err), SockOp::Bind, err, 0 };
        }
        
        return SockResult{ SockErr::None, SockOp::Bind, 0, 0 };
    }

    SockResult TCPServer::listen(int backlog) {
        if (!handle_valid()) { 
            return SockResult{ SockErr::InvalidHandle, SockOp::Listen, 0, 0 };
        }
        
        if (backlog == 0) backlog = SOMAXCONN;

        if (::listen(as_native(m_handle), backlog) != 0) {
            int err = ::WSAGetLastError();
            return SockResult{ map_err(err), SockOp::Listen, err, 0 };
        }

        return SockResult{ SockErr::None, SockOp::Listen, 0, 0 };
    }

    SockResult TCPServer::open_and_listen(uint16_t port, const char* ip) {
        auto err = open();
        if (err.code != SockErr::None) return err;

        err = bind(port, ip);
        if (err.code != SockErr::None) return err;

        return listen(0);
    }

    std::pair<std::shared_ptr<TCPClient>, SockResult> TCPServer::accept() {
        SockResult result{ SockErr::Unknown, SockOp::Accept, 0, 0 };

        if (!handle_valid()) {
            result.code = SockErr::InvalidHandle;
            return { nullptr, result };
        }

        sockaddr_in conn{};
        int conn_size = sizeof(conn);

        SOCKET sock = ::accept(as_native(m_handle), reinterpret_cast<sockaddr*>(&conn), &conn_size);
        if (sock == INVALID_SOCKET) {
            result.sys_error = ::WSAGetLastError();
            if (result.sys_error == WSAEINVAL || result.sys_error == WSAENOTSOCK) {
                result.code = SockErr::Closed;
            } else {
                result.code = map_err(result.sys_error);
            }
            return { nullptr, result };
        }

        auto client = std::make_shared<TCPClient>();
        client->adopt(from_native(sock), true);
        
        result.code = SockErr::None;
        return { std::move(client), result };
    }

    void TCPServer::request_stop() noexcept {
        close();
    }
}
#endif
