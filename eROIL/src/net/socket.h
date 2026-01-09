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

    enum class SocketIoResult : uint8_t {
        Ok,
        WouldBlock,
        Closed,
        Error
    };

    struct SocketOp {
        SocketIoResult result = SocketIoResult::Error;
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
    SocketOp make_err(int wsa_err);

    bool is_valid(socket::TCPClient s);
    bool is_valid(socket::UDP s);
    bool is_valid(socket::UDPMulti s);

    namespace tcp {
        socket::TCPClient open_tcp_socket();
        //bool set_nonblocking(socket::TCPClient sock, bool enabled);
        bool connect(socket::TCPClient sock, const char* ip, uint16_t port);
        bool shutdown_recv(socket::TCPClient sock);
        bool shutdown_both(socket::TCPClient sock);
        void close(socket::TCPClient sock);
        SocketOp send(socket::TCPClient sock, const void* data, size_t len);
        SocketOp recv(socket::TCPClient sock, void* out, size_t len);
    }

    namespace udp {
        socket::UDP open_udp_socket();
        //bool set_nonblocking(socket::UDP sock, bool enabled);
        bool bind(socket::UDP sock, const char* ip, uint16_t port);
        bool set_default_peer(socket::UDP sock, const char* ip, uint16_t port);
        void close(socket::UDP sock);
        SocketOp send_to(socket::UDP sock, const void* data, size_t len, const char* ip, uint16_t port);
        SocketOp send(socket::UDP sock, const void* data, size_t len);
        SocketOp recv_from(socket::UDP sock, void* out, size_t len, UdpEndpoint* from);
        SocketOp recv(socket::UDP sock, void* out, size_t len);
    }

    namespace udpm {
        socket::UDPMulti open_udp_socket();
        //bool set_nonblocking(socket::UDPMulti sock, bool enabled);
        bool bind(socket::UDPMulti sock, const char* ip, uint16_t port);
        bool join_multicast(socket::UDPMulti sock, const char* group_ip);
        void close(socket::UDPMulti sock);
        SocketOp send_to(socket::UDPMulti sock, const void* data, size_t len, const char* ip, uint16_t port);
        SocketOp recv_from(socket::UDPMulti sock, void* out, size_t len, UdpEndpoint* from);
    }

    class ClientSocket {
        private:
            NodeId m_connected_to;
            const char* m_ip;
            uint16_t m_port;
            socket::TCPClient m_client;

        public:
            ClientSocket();
            ~ClientSocket();

            SocketOp send(const void* buf, const size_t size);
            SocketOp recv(void* buf, const size_t size);
            void shutdown_read();
            void close();
    };
}