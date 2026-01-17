#if defined(EROIL_WIN32)
#include "socket/tcp_socket.h"
#include "windows_hdr.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <eROIL/print.h>
#include "address/address.h"

namespace eroil::sock {
    static SOCKET as_native(socket_handle handle) noexcept {
        return static_cast<SOCKET>(handle);
    }

    // static socket_handle from_native(SOCKET handle) noexcept {
    //     return static_cast<socket_handle>(handle);
    // }

    TCPClient::TCPClient() : TCPSocket(), m_dest_id(INVALID_NODE) {}

    SockResult TCPClient::connect(const char* ip, uint16_t port) {
        if (!handle_valid()) return SockResult{ SockErr::InvalidHandle, SockOp::Connect, 0, 0 };
        if (is_connected()) return SockResult{ SockErr::AlreadyConnected, SockOp::Connect, 0, 0 };
        
        // pseudo-validate ip
        if (!ip || *ip == '\0') {
            return SockResult{ SockErr::InvalidIp, SockOp::Connect, 0, 0 };
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // parse IP
        if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            return SockResult{ SockErr::InvalidIp, SockOp::Connect, 0, 0 };
        }

        if (::connect(as_native(m_handle), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            int err = ::WSAGetLastError();
            return SockResult{ map_err(err), SockOp::Connect, err, 0 };
        }

        m_connected = true;
        return SockResult{ SockErr::None, SockOp::Connect, 0, 0 };
    }

    SockResult TCPClient::send(const void* data, const size_t size) {
        if (!is_connected()) {
            return SockResult{ SockErr::NotConnected, SockOp::Send, 0, 0 };
        }

        if (size <= 0) { 
            return SockResult{ SockErr::SizeZero, SockOp::Send, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Send, 0, 0 };
        }

        // NOTE: may not always send all bytes, use send_all() for that
        int sent_bytes = 0;
        {
            std::lock_guard lock(m_send_mtx);
            sent_bytes = ::send(
                as_native(m_handle), 
                static_cast<const char*>(data), 
                static_cast<int>(size), 
                0
            );
        }

        if (sent_bytes > 0) {
            return SockResult{ SockErr::None, SockOp::Send, 0, sent_bytes };
        }

        if (sent_bytes == 0) {
            ERR_PRINT("send() sent 0 bytes, considering the socket closed");
            m_connected = false;
            return SockResult{ SockErr::Closed, SockOp::Send, 0, 0 };
        }

        int err = ::WSAGetLastError();
        if (err == WSAECONNRESET) {
            ERR_PRINT("send() errcode indicates socket closed");
            m_connected = false;
        }
        
        return SockResult{ map_err(err), SockOp::Send, err, 0 };
    }

    SockResult TCPClient::send_all(const void* data, const size_t size) {
        if (!is_connected()) {
            return SockResult{ SockErr::NotConnected, SockOp::Send, 0, 0 };
        }

        if (size <= 0) { 
            return SockResult{ SockErr::SizeZero, SockOp::Send, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Send, 0, 0 };
        }

        size_t total = 0;
        auto* ptr = static_cast<const char*>(data);

        std::lock_guard lock(m_send_mtx);
        while (total < size) {
            int to_send = static_cast<int>(size - total);
            int sent_bytes = ::send(as_native(m_handle), ptr + total, to_send, 0);

            if (sent_bytes > 0) {
                total += static_cast<size_t>(sent_bytes); 
                continue;
            }

            if (sent_bytes == 0) {
                // TOD: treating sending 0 bytes as closed, not sure if this makes sense
                m_connected = false;
                return SockResult{ SockErr::Closed, SockOp::Send, 0, static_cast<int>(total) };
            }

            int err = ::WSAGetLastError();
            if (err == WSAEINTR) {
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

        if (size <= 0) { 
            return SockResult{ SockErr::SizeZero, SockOp::Recv, 0, 0 };
        }

        if (size > static_cast<size_t>(INT32_MAX)) {
            return SockResult{ SockErr::SizeTooLarge, SockOp::Recv, 0, 0 };
        }

        int recv_bytes = ::recv(as_native(m_handle), static_cast<char*>(data), static_cast<int>(size), 0);
        if (recv_bytes > 0) {
            return SockResult{ SockErr::None, SockOp::Recv, 0, recv_bytes };
        }

        if (recv_bytes == 0) {
            // peer performed orderly shutdown
            m_connected = false;
            return SockResult{ SockErr::Closed, SockOp::Recv, 0, 0 };
        }

        int err = ::WSAGetLastError();
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

        char* out = static_cast<char*>(data);
        size_t total = 0;

        while (total < size) {
            int recv_bytes = ::recv(
                as_native(m_handle),
                out + total,
                static_cast<int>(size - total),
                0
            );

            if (recv_bytes > 0) {
                total += static_cast<size_t>(recv_bytes);
                continue;
            }

            if (recv_bytes == 0) {
                m_connected = false;
                return SockResult{
                    SockErr::Closed,
                    SockOp::Recv,
                    0,
                    static_cast<int>(total)
                };
            }

            int err = ::WSAGetLastError();
            if (err == WSAEINTR) {
                continue; // retry, we were interrupted
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