#pragma once

namespace eroil::sock {
    // use RAII to control lifetime of socket initialization
    class SocketContext {
        public:
            SocketContext();
            ~SocketContext();

            SocketContext(const SocketContext&) = delete;
            SocketContext& operator=(const SocketContext&) = delete;

            SocketContext(SocketContext&& other) = delete;
            SocketContext& operator=(SocketContext&& other) = delete;

            bool ok() const noexcept;

        private:
            bool m_ok = false;
    };

    // or control it yourself
    bool init_platform_sockets();
    void cleanup_platform_sockets();
}