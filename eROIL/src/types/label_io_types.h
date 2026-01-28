#pragma once
#include <array>
#include "const_types.h"
#include <eROIL/print.h>

namespace eroil::io {
    struct LabelInfo {
        Label label = INVALID_LABEL;
        uint32_t size = 0;

        LabelInfo() noexcept = default;
        bool operator<(const LabelInfo& other) const noexcept {
            return label < other.label;
        }
    };

    struct LabelsSnapshot {
        uint64_t gen = 0;
        std::array<LabelInfo, MAX_LABELS> labels{};
    };

    struct BroadcastMessage {
        int32_t id = INVALID_NODE;
        LabelsSnapshot send_labels{};
        LabelsSnapshot recv_labels{};
    };

    struct LabelHeader {
        uint32_t magic = 0;
        uint16_t version = 0;
        int32_t source_id = INVALID_NODE;
        uint16_t flags = 0;
        int32_t label = INVALID_LABEL;
        uint32_t data_size = 0;
        uint32_t recv_offset = 0;
    };

    enum class LabelFlag : uint16_t {
        Data = 1 << 0,
        Connect = 1 << 1,
        Disconnect = 1 << 2,
        Ping = 1 << 3,
    };

    inline bool has_flag(const uint16_t flags, const LabelFlag flag) { return flags & static_cast<std::uint16_t>(flag); }
}