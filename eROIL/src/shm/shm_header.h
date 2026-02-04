#pragma once
#include <atomic>
#include <cstddef>
#include "types/const_types.h"

namespace eroil::shm {
    enum ShmState : uint32_t {
        SHM_INITING = 0,
        SHM_READY = 1,
    };

    struct ShmHeader {
        uint32_t magic = 0;              // number we recognize
        uint32_t version = 0;            // bump when layout changes
        uint32_t _pad= 0;
        std::atomic<uint32_t> state{0};  // readiness of shm block
        size_t total_size = 0;           // total size of this memory block
    };
    static_assert(sizeof(ShmHeader) == 24);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    struct ShmMetaData {
        NodeId node_id = INVALID_NODE;
        uint32_t _pad= 0;
        size_t data_block_size;
        alignas(64) std::atomic<uint64_t> generation{0};
        alignas(64) std::atomic<uint64_t> head_bytes{0};
        alignas(64) std::atomic<uint64_t> tail_bytes{0};
        alignas(64) std::atomic<uint64_t> published_count{0}; // for debugging
    };
    static_assert(sizeof(ShmMetaData) % 64 == 0);
    static_assert(alignof(ShmMetaData) == 64);
    static_assert(std::atomic<uint64_t>::is_always_lock_free);

    enum RecordFlag : uint32_t { DROPPED = 1u << 0 };
    enum RecordState : uint32_t { WRITING = 0, COMMITTED = 1, WRAP = 2, /*CONSUMED = 3*/ };

    struct alignas(8) RecordHeader {
        std::atomic<uint32_t> state{0};
        uint32_t magic = 0;
        uint32_t flags = 0;
        uint32_t user_seq = 0;
        size_t total_size = 0;
        size_t payload_size = 0;
        uint64_t epoch = 0;
        Label label = INVALID_LABEL;
        NodeId source_id = INVALID_NODE;
    };
    static_assert(sizeof(RecordHeader) % 8 == 0);
    static_assert(sizeof(RecordHeader) == 48);

    static constexpr size_t align_up(size_t curr_size, size_t align) noexcept {
        return (curr_size + (align - 1)) & ~(align - 1);
    }

    struct ShmLayout {
        static constexpr size_t HDR_OFFSET = 0;
        static constexpr size_t META_DATA_OFFSET = align_up(sizeof(shm::ShmHeader), 64);
        static constexpr size_t DATA_BLOCK_OFFSET = align_up(META_DATA_OFFSET + sizeof(ShmMetaData), 64);
        static constexpr size_t DATA_BLOCK_SIZE = SHM_BLOCK_SIZE - DATA_BLOCK_OFFSET;
        // largest allowed position where a payload write ends (leaves enough room for wrap record header in all cases)
        static constexpr size_t DATA_USABLE_LIMIT = DATA_BLOCK_SIZE - sizeof(RecordHeader);
    };
    static_assert(ShmLayout::META_DATA_OFFSET >= sizeof(shm::ShmHeader), "meta overlaps header");
    static_assert(ShmLayout::DATA_BLOCK_OFFSET >= ShmLayout::META_DATA_OFFSET + sizeof(ShmMetaData), "data overlaps meta");
    static_assert(ShmLayout::DATA_BLOCK_OFFSET <= SHM_BLOCK_SIZE, "data offset exceeds block size");
    static_assert(ShmLayout::DATA_BLOCK_SIZE > 0, "no space for data");
    static_assert(ShmLayout::DATA_USABLE_LIMIT <= ShmLayout::DATA_BLOCK_SIZE, "usable limit invalid");

    static inline size_t get_header_offset(uint64_t pos_bytes) noexcept {
        return ShmLayout::DATA_BLOCK_OFFSET + (pos_bytes % ShmLayout::DATA_BLOCK_SIZE);
    }

    static inline size_t get_data_offset(uint64_t pos_bytes) noexcept {
        return get_header_offset(pos_bytes) + sizeof(RecordHeader);
    }

    static inline bool validate_layout(size_t block_size) noexcept {
        // this should ALWAYS return true due to static_asserts above
        // but a sanity at runtime check incase someone changes something 
        return block_size == SHM_BLOCK_SIZE;
    }
}