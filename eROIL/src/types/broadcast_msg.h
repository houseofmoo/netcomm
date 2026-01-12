#pragma once

#include <array>
#include "types.h"

namespace eroil {
    struct LabelInfo {
        Label label;
        uint32_t size;
    };

    struct BroadcastMessage {
        int id;
        std::array<LabelInfo, MAX_LABELS> send_labels;
        std::array<LabelInfo, MAX_LABELS> recv_labels;
    };
}