#include <winsock2.h>
#include <ws2tcpip.h>
#include "net/socket.h"

namespace eroil::net {
    SocketOp make_err(int wsa_err) {
        SocketOp op;
        op.result = SocketIoResult::Error;
        op.bytes = 0;
        op.sys_error = wsa_err;
        return op;
    }

    bool is_valid(socket::TCPClient s) {
        if (s.handle == 0) return false;
        SOCKET native = static_cast<SOCKET>(s.handle);
        return native != INVALID_SOCKET;
    }

    bool is_valid(socket::UDP s) {
        if (s.handle == 0) return false;
        SOCKET native = static_cast<SOCKET>(s.handle);
        return native != INVALID_SOCKET;
    }

    bool is_valid(socket::UDPMulti s) {
        if (s.handle == 0) return false;
        SOCKET native = static_cast<SOCKET>(s.handle);
        return native != INVALID_SOCKET;
    }

    bool init() {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }

    void shutdown() {
        WSACleanup();
    }
}