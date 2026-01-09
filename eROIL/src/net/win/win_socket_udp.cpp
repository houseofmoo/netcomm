#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include "net/socket.h"

namespace eroil::net::udp {
    SOCKET to_native(socket::UDP s) {
        return reinterpret_cast<SOCKET>(s.handle);
    }

    socket::UDP open_udp_socket() {
        SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        return socket::UDP{ reinterpret_cast<uintptr_t>(s) };
    }

    bool set_nonblocking(socket::UDP sock, bool enabled) {
        SOCKET s = to_native(sock);
        u_long mode = enabled ? 1UL : 0UL;
        return ::ioctlsocket(s, FIONBIO, &mode) == 0;
    }

    bool bind(socket::UDP sock, const char* ip, uint16_t port) {
        SOCKET s = to_native(sock);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (ip == nullptr || ip[0] == '\0' || std::strcmp(ip, "0.0.0.0") == 0) {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
                return false;
            }
        }

        return ::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    bool set_default_peer(socket::UDP sock, const char* ip, uint16_t port) {
        SOCKET s = to_native(sock);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) return false;

        // UDP "connect" sets default peer; no handshake happens.
        return ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    void close(socket::UDP sock) {
        if (is_valid(sock)) {
            SOCKET s = to_native(sock);
            ::closesocket(s);
        }
    }

    SocketOp send_to(socket::UDP sock, const void* data, size_t len, const char* ip, uint16_t port) {
        SOCKET s = to_native(sock);

        if (len > static_cast<size_t>(INT32_MAX)) return make_err(WSAEINVAL);

        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_port = htons(port);

        if (inet_pton(AF_INET, ip, &to.sin_addr) != 1) {
            return make_err(WSAGetLastError());
        }

        int rc = ::sendto(
            s,
            static_cast<const char*>(data),
            static_cast<int>(len),
            0,
            reinterpret_cast<const sockaddr*>(&to),
            sizeof(to)
        );

        if (rc >= 0) return SocketOp{ SocketIoResult::Ok, rc, 0 };

        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return SocketOp{ SocketIoResult::WouldBlock, 0, err };
        return make_err(err);
    }

    SocketOp send(socket::UDP sock, const void* data, size_t len) {
        SOCKET s = to_native(sock);
        if (len > static_cast<size_t>(INT32_MAX)) return make_err(WSAEINVAL);

        int rc = ::send(
            s,
            static_cast<const char*>(data),
            static_cast<int>(len),
            0
        );

        if (rc >= 0) return SocketOp{ SocketIoResult::Ok, rc, 0 };

        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return SocketOp{ SocketIoResult::WouldBlock, 0, err };
        return make_err(err);
    }

    SocketOp recv_from(socket::UDP sock, void* out, size_t len, UdpEndpoint* from) {
        SOCKET s = to_native(sock);

        if (len > static_cast<size_t>(INT32_MAX)) {
            return make_err(WSAEINVAL);
        }

        sockaddr_in src{};
        int src_len = sizeof(src);

        int rc = ::recvfrom(
            s,
            static_cast<char*>(out),
            static_cast<int>(len),
            0,
            reinterpret_cast<sockaddr*>(&src),
            &src_len
        );

        if (rc >= 0) {
            if (from) {
                from->addr_be = src.sin_addr.s_addr; // already network order
                from->port_be = src.sin_port;        // already network order
            }
            return SocketOp{ SocketIoResult::Ok, rc, 0 };
        }

        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return SocketOp{ SocketIoResult::WouldBlock, 0, err };
        return make_err(err);
    }

    SocketOp recv(socket::UDP sock, void* out, size_t len) {
        SOCKET s = to_native(sock);

        if (len > static_cast<size_t>(INT32_MAX)) {
            return make_err(WSAEINVAL);
        }

        int rc = ::recv(
            s,
            static_cast<char*>(out),
            static_cast<int>(len),
            0
        );

        if (rc >= 0) return SocketOp{ SocketIoResult::Ok, rc, 0 };

        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return SocketOp{ SocketIoResult::WouldBlock, 0, err };
        return make_err(err);
    }
}