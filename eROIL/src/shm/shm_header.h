#pragma once
#include <atomic>

namespace eroil::shm {
    struct ShmHeader {
        uint32_t magic;              // 'SHM1' little-endian
        uint16_t version;            // bump when layout changes
        uint16_t header_size;        // sizeof(ShmHeader)
        uint32_t label_size;         // size of data stored here
        std::atomic<uint32_t> state; // readiness of shm block
    };

    static constexpr uint32_t kShmMagic   = 0x314D4853u; // 'SHM1'
    static constexpr uint16_t kShmVersion = 1;

    enum ShmState : uint32_t {
        SHM_INITING = 0,
        SHM_READY   = 1,
    };
}