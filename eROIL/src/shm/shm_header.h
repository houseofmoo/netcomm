#pragma once
#include <atomic>

namespace eroil::shm {
    struct ShmHeader {
        uint32_t magic;              // magic num we set
        uint16_t version;            // bump when layout changes
        uint16_t header_size;        // sizeof(ShmHeader)
        uint32_t data_size;          // size of data stored here (label + label header)
        std::atomic<uint32_t> state; // readiness of shm block
    };

    enum ShmState : uint32_t {
        SHM_INITING = 0,
        SHM_READY   = 1,
    };
}