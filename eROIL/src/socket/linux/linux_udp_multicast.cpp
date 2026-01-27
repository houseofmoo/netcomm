#if defined(EROIL_LINUX)

#include "socket/udp_multicast.h"

#include <sys/const_types.h>
#include <sys/socket.h>
#include <netinet/in.h>   // sockaddr_in, IPPROTO_IP
#include <arpa/inet.h>    // inet_pton
#include <unistd.h>       // close
#include <errno.h>
#include <cstring>
#include <utility>

#include <eROIL/print.h>

namespace eroil::sock {
    bool UDPMulticastSocket::handle_valid() const noexcept {
        return m_handle != INVALID_SOCKET;
    }

    UDPMulticastSocket::UDPMulticastSocket()
        : m_handle(INVALID_SOCKET), m_open(false), m_joined(false), m_cfg{} {}

    UDPMulticastSocket::~UDPMulticastSocket() { 
        close(); 
    }

    UDPMulticastSocket::UDPMulticastSocket(UDPMulticastSocket&& o) noexcept
        : m_handle(std::exchange(o.m_handle, INVALID_SOCKET)),
          m_open(std::exchange(o.m_open, false)),
          m_joined(std::exchange(o.m_joined, false)),
          m_cfg(o.m_cfg) {}

    UDPMulticastSocket& UDPMulticastSocket::operator=(UDPMulticastSocket&& o) noexcept {
        if (this != &o) {
            close();
            m_handle = std::exchange(o.m_handle, INVALID_SOCKET);
            m_open   = std::exchange(o.m_open, false);
            m_joined = std::exchange(o.m_joined, false);
            m_cfg    = o.m_cfg;
        }
        return *this;
    }

    SockResult UDPMulticastSocket::open_and_join(const UdpMcastConfig& cfg) {
        SockResult result{};
        result.op = SockOp::Open;

        if (handle_valid()) { result.code = SockErr::DoubleOpen; return result; }
        if (cfg.group_ip.empty()) { result.code = SockErr::InvalidIp; return result; }
        if (cfg.bind_ip.empty()) { result.code = SockErr::InvalidIp; return result; }
        if (cfg.port == 0) { result.code = SockErr::InvalidArgument; return result; }

        int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            result.sys_error = errno;
            result.code = map_err(result.sys_error);
            return result;
        }

        m_handle = fd;
        m_open = true;
        m_cfg = cfg;

        if (cfg.reuse_addr) {
            int reuse = 1;
            ::setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &reuse, (socklen_t)sizeof(reuse));
            ::setsockopt(m_handle, SOL_SOCKET, SO_REUSEPORT, &reuse, (socklen_t)sizeof(reuse));
        }

        // bind
        result.op = SockOp::Bind;

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port   = htons(cfg.port);

        if (::inet_pton(AF_INET, cfg.bind_ip.c_str(), &local.sin_addr) != 1) {
            close();
            result.code = SockErr::InvalidIp;
            return result;
        }

        if (::bind(m_handle, reinterpret_cast<sockaddr*>(&local), (socklen_t)sizeof(local)) != 0) {
            ERR_PRINT("err ::bind()");
            result.sys_error = errno;
            result.code = map_err(result.sys_error);
            close();
            return result;
        }

        // TTL
        result.op = SockOp::Send;
        int ttl = cfg.ttl;
        if (::setsockopt(m_handle, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, (socklen_t)sizeof(ttl)) != 0) {
            result.sys_error = errno;
            result.code = map_err(result.sys_error);
            close();
            return result;
        }

        // loopback
        unsigned char loop = cfg.loopback ? 1 : 0;
        (void)::setsockopt(m_handle, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, (socklen_t)sizeof(loop));

        // join group
        result.op = SockOp::Join;
        ip_mreq mreq{};
        if (::inet_pton(AF_INET, cfg.group_ip.c_str(), &mreq.imr_multiaddr) != 1) {
            ERR_PRINT("err ::join()");
            close();
            result.code = SockErr::InvalidIp;
            return result;
        }

        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (::setsockopt(m_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, (socklen_t)sizeof(mreq)) != 0) {
            result.sys_error = errno;
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
        dst.sin_port   = htons(m_cfg.port);

        if (::inet_pton(AF_INET, m_cfg.group_ip.c_str(), &dst.sin_addr) != 1) {
            result.code = SockErr::InvalidIp;
            return result;
        }

        const ssize_t sent = ::sendto(
            m_handle,
            data,
            size,
            0,
            reinterpret_cast<sockaddr*>(&dst),
            (socklen_t)sizeof(dst)
        );

        if (sent < 0) {
            result.sys_error = errno;
            result.code = map_err(result.sys_error);
            return result;
        }

        result.bytes = static_cast<int>(sent);
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
        socklen_t srclen = (socklen_t)sizeof(src);

        const ssize_t recvd = ::recvfrom(
            m_handle,
            data,
            size,
            0,
            reinterpret_cast<sockaddr*>(&src),
            &srclen
        );

        if (recvd < 0) {
            result.sys_error = errno;
            result.code = map_err(result.sys_error);
            return result;
        }

        result.bytes = static_cast<int>(recvd);
        result.code = SockErr::None;
        return result;
    }

    void UDPMulticastSocket::close() noexcept {
        if (handle_valid()) {
            int rc;
            do { rc = ::close(m_handle); }
            while (rc != 0 && errno == EINTR);
        }
        m_handle = INVALID_SOCKET;
        m_open = false;
        m_joined = false;
    }
}
#endif
