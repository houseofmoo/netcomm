#pragma once
#include "types/const_types.h"
#include <string_view>
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

        bool ok() const noexcept {
            return code == SockErr::None;
        }

        std::string_view code_to_string() const noexcept {
            switch (code) {
                case SockErr::None: return "None";
                case SockErr::NotInitialized: return "NotInitialized";
                case SockErr::InvalidHandle: return "InvalidHandle";
                case SockErr::InvalidArgument: return "InvalidArgument";
                case SockErr::NotOpen: return "NotOpen";
                case SockErr::NotConnected: return "NotConnected";
                case SockErr::AlreadyConnected: return "AlreadyConnected";
                case SockErr::Closed: return "Closed";
                case SockErr::Shutdown: return "Shutdown";
                case SockErr::AddressInUse: return "AddressInUse";
                case SockErr::AddressNotAvailable: return "AddressNotAvailable";
                case SockErr::PermissionDenied: return "PermissionDenied";
                case SockErr::ConnectionRefused: return "ConnectionRefused";
                case SockErr::TimedOut: return "TimedOut";
                case SockErr::Unreachable: return "Unreachable";
                case SockErr::ResourceExhausted: return "ResourceExhausted";
                case SockErr::WouldBlock: return "WouldBlock";
                case SockErr::DoubleOpen: return "DoubleOpen";
                case SockErr::InvalidIp: return "InvalidIp";
                case SockErr::SizeZero: return "SizeZero";
                case SockErr::SizeTooLarge: return "SizeTooLarge";
                case SockErr::Unknown: return "Unknown";
                default: return "Unknown - error is undefined";
            }
        }

        std::string_view op_to_string() const noexcept {
            switch (op) {
                case SockOp::Configure: return "Configure";
                case SockOp::Open: return "Open"; 
                case SockOp::Bind: return "Bind";
                case SockOp::Join: return "Join";
                case SockOp::Listen: return "Listen"; 
                case SockOp::Accept: return "Accept"; 
                case SockOp::Connect: return "Connect"; 
                case SockOp::Send: return "Send";
                case SockOp::Recv: return "Recv"; 
                case SockOp::Shutdown: return "Shutdown"; 
                case SockOp::Close: return "Close";
                default: return "Unknown - op is undefined";
            }
        }
    };

    SockErr map_err(int err) noexcept;
    bool is_fatal_send_err(int e) noexcept;

    inline void print_socket_result(const SockResult& result) {
        PRINT("SOCKET RESULT:");
        PRINT("    code=", static_cast<int>(result.code));
        PRINT("    op=", static_cast<int>(result.op));
        PRINT("    sys_error=", result.sys_error);
        PRINT("    bytes=", result.bytes);
        (void)result; // in release build this function is empty, gotta appease the warning gods
    }
}