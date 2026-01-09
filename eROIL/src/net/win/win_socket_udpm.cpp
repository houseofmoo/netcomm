#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include "net/socket.h"

namespace eroil::net::udpm {
    SOCKET to_native(socket::UDPMulti s) {
        return reinterpret_cast<SOCKET>(s.handle);
    }

    socket::UDPMulti open_udp_socket() {
        SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        return socket::UDPMulti{ reinterpret_cast<uintptr_t>(s) };
    }

    bool set_nonblocking(socket::UDPMulti sock, bool enabled) {
        SOCKET s = to_native(sock);
        u_long mode = enabled ? 1UL : 0UL;
        return ::ioctlsocket(s, FIONBIO, &mode) == 0;
    }

    bool bind(socket::UDPMulti sock, const char* ip, uint16_t port) {
        SOCKET s = to_native(sock);

        BOOL reuse = TRUE;
        setsockopt(
            s, 
            SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse),
            sizeof(reuse)
        );

        BOOL loop = TRUE;
        setsockopt(
            s, 
            IPPROTO_IP, IP_MULTICAST_LOOP,
            reinterpret_cast<const char*>(&loop),
            sizeof(loop)
        );

        int ttl = 1; // local network only
        setsockopt(
            s, 
            IPPROTO_IP, IP_MULTICAST_TTL,
            reinterpret_cast<const char*>(&ttl),
            sizeof(ttl)
        );

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

    bool join_multicast(socket::UDPMulti sock, const char* group_ip) {
        SOCKET s = to_native(sock);

        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(group_ip);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        return setsockopt(
            s,
            IPPROTO_IP,
            IP_ADD_MEMBERSHIP,
            reinterpret_cast<const char*>(&mreq),
            sizeof(mreq)
        ) == 0;
    }

    void close(socket::UDPMulti sock) {
        if (is_valid(sock)) {
            SOCKET s = to_native(sock);
            ::closesocket(s);
        }
    }

    IoOp send_to(socket::UDPMulti sock, const void* data, size_t len, const char* ip, uint16_t port) {
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

        if (rc >= 0) return IoOp{ IoResult::Ok, rc, 0 };

        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return IoOp{ IoResult::WouldBlock, 0, err };
        return make_err(err);
    }

    IoOp recv_from(socket::UDPMulti sock, void* out, size_t len, UdpEndpoint* from) {
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
            return IoOp{ IoResult::Ok, rc, 0 };
        }

        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return IoOp{ IoResult::WouldBlock, 0, err };
        return make_err(err);
    }
}