#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "net/socket.h"

namespace eroil::net::tcp {
    SOCKET to_native(socket::TCPClient s) {
        return static_cast<SOCKET>(s.handle);
    }

    socket::TCPClient open_tcp_socket() {
        SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        return socket::TCPClient{ reinterpret_cast<uintptr_t>(s) };
    }

    bool set_nonblocking(socket::TCPClient sock, bool enabled) {
        SOCKET s = to_native(sock);
        u_long mode = enabled ? 1UL : 0UL;
        return ::ioctlsocket(s, FIONBIO, &mode) == 0;
    }

    bool connect(socket::TCPClient sock, const char* ip, uint16_t port) {
        SOCKET s = to_native(sock);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) return false;
        if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            int err = WSAGetLastError();
            std::cerr << "connect failed, err=" << err << "\n";
            return false;
        }

        return true;
    }

    bool shutdown_recv(socket::TCPClient sock) {
        if (!is_valid(sock)) return false;
        return ::shutdown(to_native(sock), SD_RECEIVE) == 0;
    }

    bool shutdown_both(socket::TCPClient sock) {
        if (!is_valid(sock)) return false;
        return ::shutdown(to_native(sock), SD_BOTH) == 0;
    }

    void close(socket::TCPClient sock) {
        if (is_valid(sock)) {
            SOCKET s = to_native(sock);
            closesocket(s);
        }
    }

    SocketOp send(socket::TCPClient sock, const void* data, size_t len) {
        SOCKET s = to_native(sock);

        if (len > static_cast<size_t>(INT32_MAX)) {
            return make_err(WSAEINVAL);
        }

        int rc = ::send(s, static_cast<const char*>(data), static_cast<int>(len), 0);
        if (rc > 0) {
            return SocketOp{ SocketIoResult::Ok, rc, 0 };
        }

        if (rc == 0) {
            return SocketOp{ SocketIoResult::Ok, 0, 0 };
        }

        int err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return SocketOp{ SocketIoResult::WouldBlock, 0, err };
        }
        return make_err(err);
    }

    SocketOp recv(socket::TCPClient sock, void* out, size_t len) {
        SOCKET s = to_native(sock);

        if (len > static_cast<size_t>(INT32_MAX)) {
            return make_err(WSAEINVAL);
        }

        int rc = ::recv(s, static_cast<char*>(out), static_cast<int>(len), 0);
        if (rc > 0) {
            return SocketOp{ SocketIoResult::Ok, rc, 0 };
        }

        if (rc == 0) {
            return SocketOp{ SocketIoResult::Closed, 0, 0 };
        }

        int err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return SocketOp{ SocketIoResult::WouldBlock, 0, err };
        }
        return make_err(err);
    }
}