#include "net/socket.h"

namespace eroil::net {
    ClientSocket::ClientSocket() {}

    ClientSocket::~ClientSocket() {
        close();
    }

    SocketOp ClientSocket::send(const void* buf, const size_t size) {
        return tcp::send(m_client, buf, size);
    }

    SocketOp ClientSocket::recv(void* buf, const size_t size) {
        return tcp::recv(m_client, buf, size);
    }

    void ClientSocket::shutdown_read() {
        tcp::shutdown_recv(m_client);
    }

    void ClientSocket::close() {
        tcp::close(m_client);
    }
}