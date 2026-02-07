#pragma once
#include "macros.h"
namespace eroil::sock {
    // use RAII to control lifetime of socket initialization
    class SocketContext {
        public:
            SocketContext();
            ~SocketContext();

            EROIL_NO_COPY(SocketContext)
            EROIL_NO_MOVE(SocketContext)

            bool ok() const noexcept;

        private:
            bool m_ok = false;
    };
}