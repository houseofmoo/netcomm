#pragma once
#include <cstdint>
#include <utility>

#include "types/types.h"
#include "socket_result.h"

namespace eroil::sock {

    struct UdpMcastConfig {
        const char* group_ip = "239.255.0.1"; // administratively-scoped example
        uint16_t port = 30001;
        const char* bind_ip = "0.0.0.0"; // INADDR_ANY
        int ttl = 1;
        bool loopback = true;
        bool reuse_addr = true;

        // optional: choose NIC for multicast (Windows uses interface index or local IP)
        // let OS choose default route for now
    };

    class UDPMulticastSocket final {
        private:
            socket_handle m_handle;
            bool m_open;
            bool m_joined;
            UdpMcastConfig m_cfg{};

        public:
            UDPMulticastSocket();
            ~UDPMulticastSocket();

            // do not copy
            UDPMulticastSocket(const UDPMulticastSocket&) = delete;
            UDPMulticastSocket& operator=(const UDPMulticastSocket&) = delete;

            // allow move
            UDPMulticastSocket(UDPMulticastSocket&& other) noexcept;
            UDPMulticastSocket& operator=(UDPMulticastSocket&& other) noexcept;

            bool is_open() const noexcept;
            SockResult open_and_join(const UdpMcastConfig& cfg);

            SockResult send_broadcast(const void* data, size_t size) noexcept;
            SockResult recv_broadcast(void* data, size_t size) noexcept;

            void request_stop() noexcept;
            void close() noexcept;

        private:
            bool handle_valid() const noexcept;
    };
}
