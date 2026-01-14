#include "socket/socket_result.h"
#include "windows_hdr.h"

namespace eroil::sock {
    SockErr map_err(int err) noexcept {
        switch (err) {
            // state
            case WSANOTINITIALISED: return SockErr::NotInitialized;
            case WSAENOTSOCK: return SockErr::InvalidHandle;
            case WSAEINVAL: return SockErr::InvalidArgument;
            case WSAENOTCONN: return SockErr::NotConnected;
            case WSAEISCONN: return SockErr::AlreadyConnected;
            case WSAESHUTDOWN: return SockErr::Shutdown;
            case WSAECONNRESET: return SockErr::Closed; // recv() == 0 hadnled seperately

            // bind
            case WSAEADDRINUSE: return SockErr::AddressInUse;
            case WSAEADDRNOTAVAIL: return SockErr::AddressNotAvailable;
            case WSAEACCES: return SockErr::PermissionDenied;

            // connect
            case WSAECONNREFUSED: return SockErr::ConnectionRefused;
            case WSAETIMEDOUT: return SockErr::TimedOut;
            case WSAEHOSTUNREACH: // fallthrough
            case WSAENETUNREACH: return SockErr::Unreachable;

            // resources
            case WSAENOBUFS: // fallthrough
            case WSAEMFILE: return SockErr::ResourceExhausted;

            // non-blocking
            case WSAEWOULDBLOCK: return SockErr::WouldBlock;

            default: return SockErr::Unknown;
        }
    }

    bool is_fatal_send_err(int e) noexcept {
        switch (e) {
            case WSAECONNRESET:
            case WSAENOTCONN:
            case WSAESHUTDOWN:
            case WSAECONNABORTED:
            case WSAEHOSTUNREACH:
            case WSAENETDOWN:
            case WSAENETRESET:
            case WSAENETUNREACH:
                return true;
            default:
                return false;
        }
    }
}