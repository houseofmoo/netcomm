#pragma once
#include <cstdint>
#include <utility>

#include "types/const_types.h"
#include "config/config.h"
#include "socket_result.h"
#include "types/macros.h"

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

            EROIL_NO_COPY(UDPMulticastSocket)
            EROIL_DECL_MOVE(UDPMulticastSocket)

            SockResult open_and_join(const cfg::UdpMcastConfig& cfg);
            SockResult send_broadcast(const void* data, size_t size) noexcept;
            SockResult recv_broadcast(void* data, size_t size) noexcept;
            void close() noexcept;

            // shared implementation
            bool is_open() const noexcept {
                return m_open && handle_valid();
            }
            
            void request_stop() noexcept {
                close(); // breaks blocking recvfrom
            }

        private:
            bool handle_valid() const noexcept;
    };
}
