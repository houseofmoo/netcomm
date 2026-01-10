
#include "socket/udp_multicast.h"
#include "windows_hdr.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <utility>
#include <eROIL/print.h>

namespace eroil::sock {
    static SOCKET as_native(socket_handle h) noexcept {
        return static_cast<SOCKET>(h);
    }
    static socket_handle from_native(SOCKET h) noexcept {
        return static_cast<socket_handle>(h);
    }

    
    bool UDPMulticastSocket::handle_valid() const noexcept {
         return m_handle != INVALID_SOCKET; 
    }

    UDPMulticastSocket::UDPMulticastSocket() 
        : m_handle(INVALID_SOCKET), m_open(false), m_joined(false), m_cfg{} {}

    UDPMulticastSocket::~UDPMulticastSocket() { close(); }

    UDPMulticastSocket::UDPMulticastSocket(UDPMulticastSocket&& o) noexcept
        : m_handle(std::exchange(o.m_handle, INVALID_SOCKET)),
        m_open(std::exchange(o.m_open, false)),
        m_joined(std::exchange(o.m_joined, false)),
        m_cfg(o.m_cfg) {}

    UDPMulticastSocket& UDPMulticastSocket::operator=(UDPMulticastSocket&& o) noexcept {
        if (this != &o) {
            close();
            m_handle  = std::exchange(o.m_handle, INVALID_SOCKET);
            m_open    = std::exchange(o.m_open, false);
            m_joined  = std::exchange(o.m_joined, false);
            m_cfg     = o.m_cfg;
        }
        return *this;
    }

    bool UDPMulticastSocket::is_open() const noexcept {
        return m_open && handle_valid();
    }

    SockResult UDPMulticastSocket::open_and_join(const UdpMcastConfig& cfg) {
        // open
        SockResult result{};
        result.op = SockOp::Open;

        if (handle_valid()) { result.code = SockErr::DoubleOpen; return result; }
        if (!cfg.group_ip || *cfg.group_ip == '\0') { result.code = SockErr::InvalidIp; return result; }
        if (!cfg.bind_ip  || *cfg.bind_ip  == '\0') { result.code = SockErr::InvalidIp; return result; }
        if (cfg.port == 0) { result.code = SockErr::InvalidArgument; return result; }

        SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            result.sys_error = ::WSAGetLastError();
            result.code = map_err(result.sys_error);
            return result;
        }
        m_handle = from_native(sock);
        m_open = true;
        m_cfg = cfg;

        if (cfg.reuse_addr) {
            BOOL reuse = TRUE;
            ::setsockopt(as_native(m_handle), SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        }

        // bind
        result.op = SockOp::Bind;

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(cfg.port);

        if (::inet_pton(AF_INET, cfg.bind_ip, &local.sin_addr) != 1) {
            close();
            result.code = SockErr::InvalidIp;
            return result;
        }

        if (::bind(as_native(m_handle), reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
            ERR_PRINT("err ::bind()");
            result.sys_error = ::WSAGetLastError();
            result.code = map_err(result.sys_error);
            close();
            return result;
        }

        result.op = SockOp::Send;
        int ttl = cfg.ttl;
        if (::setsockopt(as_native(m_handle), IPPROTO_IP, IP_MULTICAST_TTL,
                        reinterpret_cast<const char*>(&ttl), sizeof(ttl)) != 0) {
            result.sys_error = ::WSAGetLastError();
            result.code = map_err(result.sys_error);
            close();
            return result;
        }

        // loopback
        u_char loop = cfg.loopback ? 1 : 0;
        ::setsockopt(as_native(m_handle), IPPROTO_IP, IP_MULTICAST_LOOP,
                    reinterpret_cast<const char*>(&loop), sizeof(loop));

        // join group
        result.op = SockOp::Join;
        ip_mreq mreq{};
        if (::inet_pton(AF_INET, cfg.group_ip, &mreq.imr_multiaddr) != 1) {
            ERR_PRINT("err ::join()");
            close();
            result.code = SockErr::InvalidIp;
            return result;
        }
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (::setsockopt(as_native(m_handle), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        reinterpret_cast<const char*>(&mreq), sizeof(mreq)) != 0) {
            result.sys_error = ::WSAGetLastError();
            result.code = map_err(result.sys_error);
            close();
            return result;
        }

        m_joined = true;
        result.code = SockErr::None;
        result.op = SockOp::Open;
        return result;
    }

    SockResult UDPMulticastSocket::send_broadcast(const void* data, size_t size) noexcept {
        SockResult result{};
        result.op = SockOp::Send;

        if (!is_open() || !m_joined) { result.code = SockErr::NotOpen; return result; }
        if (!data) { result.code = SockErr::InvalidArgument; return result; }
        if (size == 0) { result.code = SockErr::SizeZero; return result; }
        if (size > static_cast<size_t>(INT32_MAX)) { result.code = SockErr::SizeTooLarge; return result; }

        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_port = htons(m_cfg.port);

        if (::inet_pton(AF_INET, m_cfg.group_ip, &dst.sin_addr) != 1) {
            result.code = SockErr::InvalidIp;
            return result;
        }

        int sent = ::sendto(as_native(m_handle),
                            static_cast<const char*>(data),
                            static_cast<int>(size),
                            0,
                            reinterpret_cast<sockaddr*>(&dst),
                            sizeof(dst));

        if (sent == SOCKET_ERROR) {
            result.sys_error = ::WSAGetLastError();
            result.code = map_err(result.sys_error);
            return result;
        }

        result.bytes = sent;
        result.code = SockErr::None;
        return result;
    }

    SockResult UDPMulticastSocket::recv_broadcast(void* data, size_t size) noexcept {
        SockResult result{};
        result.op = SockOp::Recv;

        if (!is_open() || !m_joined) { result.code = SockErr::NotOpen; return result; }
        if (!data) { result.code = SockErr::InvalidArgument; return result; }
        if (size == 0) { result.code = SockErr::SizeZero; return result; }
        if (size > static_cast<size_t>(INT32_MAX)) { result.code = SockErr::SizeTooLarge; return result; }

        sockaddr_in src{};
        int srclen = sizeof(src);

        int recvd = ::recvfrom(as_native(m_handle),
                            static_cast<char*>(data),
                            static_cast<int>(size),
                            0,
                            reinterpret_cast<sockaddr*>(&src),
                            &srclen);

        if (recvd == SOCKET_ERROR) {
            result.sys_error = ::WSAGetLastError();
            result.code = map_err(result.sys_error);
            return result;
        }

        result.bytes = recvd;
        result.code = SockErr::None;
        return result;
    }

    void UDPMulticastSocket::request_stop() noexcept {
        close(); // breaks blocking recvfrom
    }

    void UDPMulticastSocket::close() noexcept {
        if (handle_valid()) {
            ::closesocket(as_native(m_handle));
        }
        m_handle = INVALID_SOCKET;
        m_open = false;
        m_joined = false;
    }
}