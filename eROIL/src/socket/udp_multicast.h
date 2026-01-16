#pragma once
#include <cstdint>
#include <utility>

#include "types/types.h"
#include "config/config.h"
#include "socket_result.h"

namespace eroil::sock {
    class UDPMulticastSocket final {
        private:
            socket_handle m_handle;
            bool m_open;
            bool m_joined;
            cfg::UdpMcastConfig m_cfg{};

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
            SockResult open_and_join(const cfg::UdpMcastConfig& cfg);

            SockResult send_broadcast(const void* data, size_t size) noexcept;
            SockResult recv_broadcast(void* data, size_t size) noexcept;

            void request_stop() noexcept;
            void close() noexcept;

        private:
            bool handle_valid() const noexcept;
    };
}
