#pragma once
#include <atomic>
#include <memory>
#include "types/types.h"

namespace eroil::shm {
    struct ShmHeader {
        uint32_t magic = 0;                        // number we recognize
        uint32_t version = 0;                      // bump when layout changes
        uint32_t total_size = 0;                   // total size of this memory block
        std::atomic<uint32_t> state{SHM_INITING};  // readiness of shm block
    };
    static_assert(sizeof(ShmHeader) == 16); // 128 bits

    enum ShmState : uint32_t {
        SHM_INITING = 0,
        SHM_READY = 1,
    };

    struct ShmMetaData {
        uint32_t magic = 12345;
        uint32_t version = 1;
        uint64_t generation = 0;
        NodeId node_id = INVALID_NODE; // id of the node this belongs to

        uint32_t descriptor_capacity = 0; // num slots in descriptor ring (pow of 2)
        uint32_t descriptor_mask = 0;   // cap - 1
        uint32_t descriptor_region_offset = 0;
        uint32_t descriptor_region_size = 0;

        uint32_t blob_region_offset = 0;
        uint32_t blob_region_size = 0;
        uint32_t _pad = 0;

        alignas(64) std::atomic<uint64_t> write_ticket{0}; // next sequence to claim
        alignas(64) std::atomic<uint64_t> read_ticket{0}; // next sequence consumer reads
        alignas(64) std::atomic<uint64_t> published_count{0}; // for debugging

        alignas(64) std::atomic<uint64_t> blob_head_bytes{0}; // producer alloc head (offset into blob region)
        alignas(64) std::atomic<uint64_t> blob_tail_bytes{0}; // consumer free tail
    };
    static_assert(sizeof(ShmMetaData) % 64 == 0); // with alignment expands to 384 bytes
    static_assert(sizeof(ShmMetaData) == 384); // just for myself to see if the size is changing

    static constexpr uint32_t SHM_FLAG_DROPPED = 1u << 0;

    struct alignas(64) ShmBlockDescriptor {
        std::atomic<uint64_t> slot_seq{0};

        // meta data
        NodeId source_id = INVALID_NODE;
        Label label = INVALID_LABEL;
        uint32_t flags = 0;

        // blobl info
        uint32_t blob_offset = 0;
        uint32_t blob_size = 0;

        uint64_t user_seq = 0;
        uint64_t _pad = 0;
    };
    static_assert(sizeof(ShmBlockDescriptor) == 64);

    static constexpr size_t align_up(size_t curr_size, size_t align) noexcept {
        return (curr_size + (align - 1)) & ~(align - 1);
    }
    static_assert((64 & (64 - 1)) == 0);
    static_assert((4096 & (4096 - 1)) == 0);

    struct ShmLayout {
        static constexpr std::size_t HDR_OFFSET = 0;

        static constexpr std::size_t META_DATA_OFFSET = align_up(sizeof(shm::ShmHeader), 64);

        static constexpr std::size_t DESC_OFFSET = align_up(META_DATA_OFFSET + sizeof(ShmMetaData), 64);
        static constexpr std::size_t DESC_COUNT = SHM_DESCRIPTOR_RING_SIZE / sizeof(ShmBlockDescriptor);

        static constexpr std::size_t BLOB_OFFSET =  align_up(DESC_OFFSET + SHM_DESCRIPTOR_RING_SIZE, 4096);
        static constexpr std::size_t BLOB_SIZE = (SHM_BLOCK_SIZE > BLOB_OFFSET) ? (SHM_BLOCK_SIZE - BLOB_OFFSET) : 0;
    };
    static_assert(SHM_DESCRIPTOR_RING_SIZE % sizeof(ShmBlockDescriptor) == 0);
    static_assert(ShmLayout::BLOB_SIZE > 0);
    static_assert(ShmLayout::BLOB_OFFSET < SHM_BLOCK_SIZE);
    static_assert(ShmLayout::DESC_COUNT == 131072);
    static_assert(ShmLayout::DESC_OFFSET >= ShmLayout::META_DATA_OFFSET + sizeof(ShmMetaData));
    static_assert(ShmLayout::BLOB_OFFSET >= ShmLayout::DESC_OFFSET + SHM_DESCRIPTOR_RING_SIZE);


    struct ShmRecvBlob {
        NodeId source_id = INVALID_NODE;
        Label label = INVALID_LABEL;
        uint64_t user_seq = 0;
        size_t buf_size = 0;
        std::unique_ptr<std::byte[]> buf = nullptr;
    };

    struct ShmSendBlob {
        NodeId source_id = INVALID_NODE;
        Label label = INVALID_LABEL;
        uint64_t user_seq = 0;
        size_t buf_size = 0;
        std::unique_ptr<std::byte[]> buf = nullptr;
    };
}