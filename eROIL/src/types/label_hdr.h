#pragma once

#include "types/types.h"

namespace eroil {
    struct LabelHeader {
        uint32_t magic;
        uint16_t version;
        int32_t source_id;
        uint16_t flags;
        uint32_t label;
        uint32_t data_size;
    };

    enum class LabelFlag : uint16_t {
        Data = 1 << 0,
        Connect = 1 << 1,
        Disconnect = 1 << 2
    };

    inline void set_flag(uint16_t& flags, const LabelFlag flag) { flags |= static_cast<uint16_t>(flag); }
    inline bool has_flag(const uint16_t flags, const LabelFlag flag) { return flags & static_cast<uint16_t>(flag); }
}