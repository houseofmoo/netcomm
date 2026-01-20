#pragma once
#include <string>
#include <mutex>
#include <memory>
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

            // do not copy
            TCPSocket(const TCPSocket&) = delete;
            TCPSocket& operator=(const TCPSocket&) = delete;

            // allow move
            TCPSocket(TCPSocket&& other) noexcept;
            TCPSocket& operator=(TCPSocket&& other) noexcept;

            SockResult open();
            void shutdown() noexcept;
            void close() noexcept;

            // shared implementation
            bool is_connected() const noexcept { 
                return m_connected && handle_valid(); 
            }

            void adopt(socket_handle handle, bool connected = true) noexcept {
                close();
                m_handle = handle;
                m_connected = connected;
            }

            void disconnect() noexcept {
                shutdown();
                close();
            }


        protected:
            bool handle_valid() const noexcept;
    };

    class TCPClient final : public TCPSocket {
        private:
            NodeId m_dest_id;

            // to prevent any chance of a send being interrupted and data arriving at
            // destination out of order, we lock on sends. We do not need to lock
            // on recvs since only 1 thread listens to each sockets incoming messages
            std::mutex m_send_mtx; 

        public:
            TCPClient();
            ~TCPClient() = default;

            // do not copy
            TCPClient(const TCPClient&) = delete;
            TCPClient& operator=(const TCPClient&) = delete;

            // do not allow move (due to mutex)
            TCPClient(TCPClient&&) noexcept = delete;
            TCPClient& operator=(TCPClient&&) noexcept = delete;

            void set_destination_id(NodeId dest_id) { m_dest_id = dest_id; };
            NodeId get_destination_id() { return m_dest_id; }

            SockResult connect(const char* ip, uint16_t port);
            SockResult send(const void* data, const size_t size);
            SockResult send_all(const void* data, const size_t size);
            SockResult recv(void* data, const size_t size);
            SockResult recv_all(void* data, const size_t size);

            // shared implementation
            SockResult open_and_connect(const char* ip, uint16_t port) {
                const auto open_err = open();
                if (open_err.code != SockErr::None) {
                    return open_err;
                }
                return connect(ip, port);
            }
    };

    class TCPServer final : public TCPSocket {
        public:
            TCPServer() = default;
            ~TCPServer() = default;

            // do not copy
            TCPServer(const TCPServer&) = delete;
            TCPServer& operator=(const TCPServer&) = delete;

            // allow move
            TCPServer(TCPServer&&) noexcept = default;
            TCPServer& operator=(TCPServer&&) noexcept = default;

            SockResult bind(uint16_t port, const char* ip = "0.0.0.0");
            SockResult listen(int backlog = 0);
            std::pair<std::shared_ptr<TCPClient>, SockResult> accept();

            // shared implementation
            SockResult open_and_listen(uint16_t port, const char* ip = "0.0.0.0") {
                auto err = open();
                if (err.code != SockErr::None) return err;

                err = bind(port, ip);
                if (err.code != SockErr::None) return err;

                return listen(0);
            }
            
            void request_stop() noexcept { close(); }

    };
}
