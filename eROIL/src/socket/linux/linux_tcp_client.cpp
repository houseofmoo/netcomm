#if defined(EROIL_LINUX)

#include "socket/tcp_socket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <eROIL/print.h>

namespace eroil::sock {
    TCPClient::TCPClient() : TCPSocket(), m_dest_id(INVALID_NODE) {}

    SockResult TCPClient::connect(const char* ip, uint16_t port) {
        if (!handle_valid()) return SockResult{ SockErr::InvalidHandle, SockOp::Connect, 0, 0 };
        if (is_connected())  return SockResult{ SockErr::AlreadyConnected, SockOp::Connect, 0, 0 };

        if (!ip || *ip == '\0') {
            return SockResult{ SockErr::InvalidIp, SockOp::Connect, 0, 0 };
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);

        if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            return SockResult{ SockErr::InvalidIp, SockOp::Connect, 0, 0 };
        }

        if (::connect(m_handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            const int err = errno;
            return SockResult{ map_err(err), SockOp::Connect, err, 0 };
        }

        m_connected = true;
        return SockResult{ SockErr::None, SockOp::Connect, 0, 0 };
    }

    SockResult TCPClient::open_and_connect(const char* ip, uint16_t port) {
        const auto open_err = open();
        if (open_err.code != SockErr::None) {
            return open_err;
        }
        return connect(ip, port);
    }

    SockResult TCPClient::send(const void* data, const size_t size) {
        if (!is_connected()) {
            return SockResult{ SockErr::NotConnected, SockOp::Send, 0, 0 };
        }

        if (size == 0) {
            return SockResult{ SockErr::SizeZero, SockOp::Send, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Send, 0, 0 };
        }

        // NOTE: may not always send all bytes, use send_all() for that
        ssize_t sent_bytes = 0;
        {
            std::lock_guard lock(m_send_mtx);
            sent_bytes = ::send(
                m_handle,
                data,
                size,
                MSG_NOSIGNAL
            );
        }

        if (sent_bytes > 0) {
            return SockResult{ SockErr::None, SockOp::Send, 0, static_cast<int>(sent_bytes) };
        }

        if (sent_bytes == 0) {
            // This is unusual for send(); treat as closed to match your Windows logic.
            ERR_PRINT("send() sent 0 bytes, considering the socket closed");
            m_connected = false;
            return SockResult{ SockErr::Closed, SockOp::Send, 0, 0 };
        }

        const int err = errno;

        // Match your Windows behavior: mark disconnected on fatal errors.
        if (is_fatal_send_err(err)) {
            ERR_PRINT("send() fatal error indicates socket closed/unusable, errno=", err);
            m_connected = false;
        }

        return SockResult{ map_err(err), SockOp::Send, err, 0 };
    }

    SockResult TCPClient::send_all(const void* data, const size_t size) {
        if (!is_connected()) {
            return SockResult{ SockErr::NotConnected, SockOp::Send, 0, 0 };
        }

        if (size == 0) {
            return SockResult{ SockErr::SizeZero, SockOp::Send, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Send, 0, 0 };
        }

        size_t total = 0;
        const auto* ptr = static_cast<const std::byte*>(data);

        std::lock_guard lock(m_send_mtx);
        while (total < size) {
            const size_t remaining = size - total;

            // send() takes size_t on Linux, returns ssize_t
            const ssize_t sent = ::send(
                m_handle,
                ptr + total,
                remaining,
                MSG_NOSIGNAL
            );

            if (sent > 0) {
                total += static_cast<size_t>(sent);
                continue;
            }

            if (sent == 0) {
                // TOD: treating sending 0 bytes as closed, not sure if this makes sense
                m_connected = false;
                return SockResult{ SockErr::Closed, SockOp::Send, 0, static_cast<int>(total) };
            }

            const int err = errno;
            if (err == EINTR) {
                continue; // retry
            }

            if (is_fatal_send_err(err)) {
                m_connected = false;
            }

            return SockResult{ map_err(err), SockOp::Send, err, static_cast<int>(total) };
        }

        return SockResult{ SockErr::None, SockOp::Send, 0, static_cast<int>(total) };
    }

    SockResult TCPClient::recv(void* data, const size_t size) {
        if (!is_connected()) {
            return SockResult{ SockErr::NotConnected, SockOp::Recv, 0, 0 };
        }

        if (size == 0) {
            return SockResult{ SockErr::SizeZero, SockOp::Recv, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Recv, 0, 0 };
        }

        ssize_t recv_bytes = ::recv(
            m_handle,
            data,
            size,
            0
        );

        if (recv_bytes > 0) {
            return SockResult{ SockErr::None, SockOp::Recv, 0, static_cast<int>(recv_bytes) };
        }

        if (recv_bytes == 0) {
            // peer performed orderly shutdown
            m_connected = false;
            return SockResult{ SockErr::Closed, SockOp::Recv, 0, 0 };
        }

        const int err = errno;
        return SockResult{ map_err(err), SockOp::Recv, err, 0 };
    }

    SockResult TCPClient::recv_all(void* data, const size_t size) {
        if (!is_connected()) {
            return SockResult{ SockErr::NotConnected, SockOp::Recv, 0, 0 };
        }

        if (size == 0) {
            return SockResult{ SockErr::SizeZero, SockOp::Recv, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Recv, 0, 0 };
        }

        auto* out = static_cast<std::byte*>(data);
        size_t total = 0;

        while (total < size) {
            const ssize_t r = ::recv(
                m_handle,
                out + total,
                size - total,
                0
            );

            if (r > 0) {
                total += static_cast<size_t>(r);
                continue;
            }

            if (r == 0) {
                m_connected = false;
                return SockResult{
                    SockErr::Closed,
                    SockOp::Recv,
                    0,
                    static_cast<int>(total)
                };
            }

            const int err = errno;

            if (err == EINTR) {
                continue; // retry
            }

            return SockResult{
                map_err(err),
                SockOp::Recv,
                err,
                static_cast<int>(total)
            };
        }

        return SockResult{
            SockErr::None,
            SockOp::Recv,
            0,
            static_cast<int>(total)
        };
    }

}
#endif
