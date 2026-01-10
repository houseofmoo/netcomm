#pragma once
#include <string>
#include "types/types.h"
#include "socket_result.h"

namespace eroil::sock {
    class TCPSocket {
        protected:
            socket_handle m_handle;
            bool m_connected;

        public:
            TCPSocket();
            ~TCPSocket();

            TCPSocket(const TCPSocket&) = delete;
            TCPSocket& operator=(const TCPSocket&) = delete;

            TCPSocket(TCPSocket&& other) noexcept;
            TCPSocket& operator=(TCPSocket&& other) noexcept;

            bool is_connected() const noexcept;
            void adopt(socket_handle handle, bool connected = true) noexcept;
            SockResult open();
            void disconnect() noexcept;
            void shutdown() noexcept;
            void close() noexcept;

        protected:
            bool handle_valid() const noexcept;
    };

    class TCPClient final : public TCPSocket {
        public:
            TCPClient() = default;
            ~TCPClient() = default;

            TCPClient(const TCPClient&) = delete;
            TCPClient& operator=(const TCPClient&) = delete;

            TCPClient(TCPClient&&) noexcept = default;
            TCPClient& operator=(TCPClient&&) noexcept = default;

            SockResult connect(const char* ip, uint16_t port);
            SockResult open_and_connect(const char* ip, uint16_t port);
            SockResult send(const void* data, const size_t size);
            SockResult send_all(const void* data, const size_t size);
            SockResult recv(void* data, const size_t size);
            SockResult recv_all(void* data, const size_t size);
    };

    class TCPServer final : public TCPSocket {
        public:
            TCPServer() = default;
            ~TCPServer() = default;

            TCPServer(const TCPServer&) = delete;
            TCPServer& operator=(const TCPServer&) = delete;

            TCPServer(TCPServer&&) noexcept = default;
            TCPServer& operator=(TCPServer&&) noexcept = default;

            SockResult bind(uint16_t port, const char* ip = "0.0.0.0");
            SockResult listen(int backlog = 0);
            SockResult open_and_listen(uint16_t port, const char* ip = "0.0.0.0");

            std::pair<TCPClient, SockResult> accept();
            void request_stop() noexcept;
    };
}
