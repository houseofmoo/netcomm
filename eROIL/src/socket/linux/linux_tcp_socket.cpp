#if defined(EROIL_LINUX)

#include "socket/tcp_socket.h"
#include <utility>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>   // AF_INET
#include <netinet/tcp.h>  // IPPROTO_TCP
#include <unistd.h>       // close()
#include <errno.h>

namespace eroil::sock {
    static int as_native(socket_handle handle) noexcept {
        return static_cast<int>(handle);
    }

    static socket_handle from_native(int fd) noexcept {
        return static_cast<socket_handle>(fd);
    }

    TCPSocket::TCPSocket() : m_handle(from_native(INVALID_SOCKET)), m_connected(false) {}

    TCPSocket::~TCPSocket() {
        disconnect();
    }

    TCPSocket::TCPSocket(TCPSocket&& other) noexcept
        : m_handle(std::exchange(other.m_handle, from_native(INVALID_SOCKET))),
          m_connected(std::exchange(other.m_connected, false)) {}

    TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = std::exchange(other.m_handle, from_native(INVALID_SOCKET));
            m_connected = std::exchange(other.m_connected, false);
        }
        return *this;
    }

    bool TCPSocket::handle_valid() const noexcept {
        return as_native(m_handle) != INVALID_SOCKET;
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
        if (handle_valid()) {
            return SockResult{ SockErr::DoubleOpen, SockOp::Open, 0, 0 };
        }

        int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd >= 0) {
            m_handle = from_native(fd);
            return SockResult{ SockErr::None, SockOp::Open, 0, 0 };
        }

        const int err = errno;
        return SockResult{ map_err(err), SockOp::Open, err, 0 };
    }

    void TCPSocket::disconnect() noexcept {
        shutdown();
        close();
    }

    void TCPSocket::shutdown() noexcept {
        if (handle_valid()) {
            ::shutdown(as_native(m_handle), SHUT_RDWR);
        }
        m_connected = false;
    }

    void TCPSocket::close() noexcept {
        if (handle_valid()) {
            ::close(as_native(m_handle));
        }
        m_handle = from_native(INVALID_SOCKET);
        m_connected = false;
    }

} // namespace eroil::sock

#endif // EROIL_LINUX
