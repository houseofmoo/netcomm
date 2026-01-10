#include "socket/tcp_socket.h"
#include "windows_hdr.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <utility>

namespace eroil::sock {
    static SOCKET as_native(socket_handle handle) noexcept {
        return static_cast<SOCKET>(handle);
    }

    static socket_handle from_native(SOCKET handle) noexcept {
        return static_cast<socket_handle>(handle);
    }

    TCPSocket::TCPSocket() : m_handle(INVALID_SOCKET), m_connected(false) {}

    TCPSocket::~TCPSocket() {
        disconnect();
    }

    TCPSocket::TCPSocket(TCPSocket&& other) noexcept 
        : m_handle(std::exchange(other.m_handle, INVALID_SOCKET)), 
          m_connected(std::exchange(other.m_connected, false)) {}

    TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = std::exchange(other.m_handle, INVALID_SOCKET);
            m_connected = std::exchange(other.m_connected, false);
        }
        return *this;
    }

    bool TCPSocket::handle_valid() const noexcept {
        return m_handle != INVALID_SOCKET;
    }

    bool TCPSocket::is_connected() const noexcept {
        return m_connected && handle_valid();
    }

    void TCPSocket::adopt(socket_handle handle, bool connected) noexcept {
        close();
        m_handle = handle;
        m_connected = connected;
    }

    SockResult TCPSocket::open() {
        if (handle_valid()) return SockResult{ SockErr::DoubleOpen, SockOp::Open, 0, 0 };

        m_handle = from_native(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (handle_valid()) {
            return SockResult{ SockErr::None, SockOp::Open, 0, 0 };
        }

        int err = ::WSAGetLastError();
        return SockResult{ map_err(err), SockOp::Open, err, 0 };
    }

    void TCPSocket::disconnect() noexcept {
        shutdown();
        close();
    }

    void TCPSocket::shutdown() noexcept {
        if (handle_valid()) {
            ::shutdown(as_native(m_handle), SD_BOTH);
        }
        m_connected = false;
    }

    void TCPSocket::close() noexcept {
        if (handle_valid()) {
            ::closesocket(as_native(m_handle));
        }
        m_handle = INVALID_SOCKET;
        m_connected = false;
    }
}