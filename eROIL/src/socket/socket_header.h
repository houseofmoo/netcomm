#pragma once

#include "types/types.h"

namespace eroil::sock {
    struct SocketHeader {
        uint32_t magic;
        uint16_t version;
        int32_t source_id;
        int32_t dest_id;
        uint16_t flags;
        uint32_t label;
        uint32_t data_size;
    };

    static constexpr uint32_t kMagic = 0x4C4F5245u; // 'EROL'
    static constexpr uint16_t kWireVer = 1;
    static constexpr size_t kMaxPayload = 1u << 20; // 1 MB

    enum class SocketFlag : uint16_t {
        Data = 1 << 0,
        Connect = 1 << 1,
        Disconnect = 1 << 2
    };

    inline void set_flag(uint16_t& flags, const SocketFlag flag) { flags |= static_cast<uint16_t>(flag); }
    inline bool has_flag(const uint16_t flags, const SocketFlag flag) { return flags & static_cast<uint16_t>(flag); }
}