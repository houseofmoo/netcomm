#pragma once

#include "types/types.h"

namespace eroil::net {
    namespace socket {
        struct TCPClient {
            uintptr_t handle = 0;
        };

        struct UDP {
            uintptr_t handle = 0;
        };

        struct UDPMulti {
            uintptr_t handle = 0;
        };
    }

    enum class IoResult : uint8_t {
        Ok,
        WouldBlock,
        Closed,
        Error
    };

    struct IoOp {
        IoResult result = IoResult::Error;
        int bytes = 0;          // number of bytes sent/received (0 allowed)
        int sys_error = 0;      // platform error code
    };

    struct UdpEndpoint {
        uint32_t addr_be = 0; 
        uint16_t port_be = 0;
    };

    // automatically setup and teardown context for sockets, otherwise use init/shutdown
    class NetContext {
        public:
            NetContext();
            ~NetContext();

            NetContext(const NetContext&) = delete;
            NetContext& operator=(const NetContext&) = delete;

            NetContext(NetContext&& other) noexcept;
            NetContext& operator=(NetContext&& other) noexcept;

            bool ok() const noexcept;

        private:
            bool m_ok = false;
    };

    bool init();
    void shutdown();
    IoOp make_err(int wsa_err);

    bool is_valid(socket::TCPClient s);
    bool is_valid(socket::UDP s);
    bool is_valid(socket::UDPMulti s);

    namespace tcp {
        socket::TCPClient open_tcp_socket();
        bool set_nonblocking(socket::TCPClient sock, bool enabled);
        bool connect(socket::TCPClient sock, const char* ip, uint16_t port);
        void close(socket::TCPClient sock);
        IoOp send(socket::TCPClient sock, const void* data, size_t len);
        IoOp recv(socket::TCPClient sock, void* out, size_t len);
    }

    namespace udp {
        socket::UDP open_udp_socket();
        bool set_nonblocking(socket::UDP sock, bool enabled);
        bool bind(socket::UDP sock, const char* ip, uint16_t port);
        bool set_default_peer(socket::UDP sock, const char* ip, uint16_t port);
        void close(socket::UDP sock);
        IoOp send_to(socket::UDP sock, const void* data, size_t len, const char* ip, uint16_t port);
        IoOp send(socket::UDP sock, const void* data, size_t len);
        IoOp recv_from(socket::UDP sock, void* out, size_t len, UdpEndpoint* from);
        IoOp recv(socket::UDP sock, void* out, size_t len);
    }

    namespace udpm {
        socket::UDPMulti open_udp_socket();
        bool set_nonblocking(socket::UDPMulti sock, bool enabled);
        bool bind(socket::UDPMulti sock, const char* ip, uint16_t port);
        bool join_multicast(socket::UDPMulti sock, const char* group_ip);
        void close(socket::UDPMulti sock);
        IoOp send_to(socket::UDPMulti sock, const void* data, size_t len, const char* ip, uint16_t port);
        IoOp recv_from(socket::UDPMulti sock, void* out, size_t len, UdpEndpoint* from);
    }

    class ClientSocket {
        private:
            NodeId m_connected_to;
            const char* ip;
            uint16_t port;
            socket::TCPClient client;

        public:
            ClientSocket();
            ~ClientSocket();

            void send(const void* buf, const size_t size);
            void recv(void* buf, const size_t size);
    };
}