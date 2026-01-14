#pragma once
#include "types/types.h"
#include <eROIL/print.h>

namespace eroil::sock {
    enum class SockErr: uint8_t {
        None,

        NotInitialized,      // WSAStartup missing (windows)
        InvalidHandle,       // not a socket / invalid
        InvalidArgument,     // bad params, invalid IP, etc.
 
        NotOpen,             // your state: no socket created
        NotConnected,        // your state
        AlreadyConnected,    // your state
        Closed,              // orderly close (recv=0) or local close
        Shutdown,            // local shutdown called
 
        AddressInUse,        // bind
        AddressNotAvailable, // bind/connect destination issues
        PermissionDenied,    // bind priv ports / policy
        ConnectionRefused,   // connect
        TimedOut,            // connect / maybe recv if timeouts configured
        Unreachable,         // host/net unreachable
        ResourceExhausted,   // ENOBUFS/EMFILE etc.
 
        WouldBlock,          // future non-blocking
 
        DoubleOpen,          // other checks we do internally unrelated to system errors
        InvalidIp,
        SizeZero,
        SizeTooLarge,
        Unknown,
    };

    enum class SockOp : uint8_t {
        Configure,
        Open, 
        Bind,
        Join,
        Listen, 
        Accept, 
        Connect, 
        Send,
        Recv, 
        Shutdown, 
        Close 
    };

    struct SockResult {
        SockErr code = SockErr::Unknown;
        SockOp op = SockOp::Open;
        int32_t sys_error = 0;     // WSAGetLastError() / errno
        int32_t bytes = 0;         // only meaningful for send/recv
    };

    SockErr map_err(int err) noexcept;
    bool is_fatal_send_err(int e) noexcept;

    inline void print_socket_result(const SockResult& result) {
        PRINT("SOCKET RESULT:");
        PRINT("    code=", (int)result.code);
        PRINT("    op=", (int)result.op);
        PRINT("    sys_error=", result.sys_error);
        PRINT("    bytes=", result.bytes);
    }
}