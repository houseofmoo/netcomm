#include <winsock2.h>
#include <ws2tcpip.h>
#include "net/socket.h"

namespace eroil::net {
    IoOp make_err(int wsa_err) {
        IoOp op;
        op.result = IoResult::Error;
        op.bytes = 0;
        op.sys_error = wsa_err;
        return op;
    }

    bool is_valid(socket::TCPClient s) {
        return s.handle != 0 && s.handle != INVALID_SOCKET;
    }

    bool is_valid(socket::UDP s) {
        return s.handle != 0 && s.handle != INVALID_SOCKET;
    }

    bool is_valid(socket::UDPMulti s) {
        return s.handle != 0 && s.handle != INVALID_SOCKET;
    }

    bool init() {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }

    void shutdown() {
        WSACleanup();
    }
}