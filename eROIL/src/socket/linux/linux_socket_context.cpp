#ifdef EROIL_LINUX
#include "socket/socket_context.h"
        #include <signal.h>

namespace eroil::sock {
    SocketContext::SocketContext() : m_ok(true) {
        // prevent terminanting processes when a send()
        // on closed socket occurs
        //signal(SIGPIPE, SIG_IGN);

        // disabled this to not pollute the application as a whole,
        // same result adding MSG_NOSIGNAL to send() calls
    }

    SocketContext::~SocketContext() {
        m_ok = false;
    }

    bool SocketContext::ok() const noexcept {
        return m_ok;
    }

    bool init_platform_sockets() {}
    void cleanup_platform_sockets() {}
}
#endif
