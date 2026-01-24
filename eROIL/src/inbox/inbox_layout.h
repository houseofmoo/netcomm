// #pragma once
// #include "types/types.h"
// #include "inbox_header.h"
// #include "shm/shm_header.h"

// namespace eroil::ibx {
//     static constexpr size_t align_up(size_t curr_size, size_t align) noexcept {
//         return (curr_size + (align - 1)) & ~(align - 1);
//     }

//     struct InboxLayout {
//         static constexpr std::size_t SHM_HDR_OFFSET = 0;

//         static constexpr std::size_t INBOX_HDR_OFFSET = align_up(sizeof(shm::ShmHeader), 64);

//         static constexpr std::size_t DESC_OFFSET = align_up(INBOX_HDR_OFFSET + sizeof(InboxHeader), 64);
//         static constexpr std::size_t DESC_COUNT = SHM_DESCRIPTOR_RING_SIZE / sizeof(InboxDescriptor);

//         static constexpr std::size_t BLOB_OFFSET =  align_up(DESC_OFFSET + SHM_DESCRIPTOR_RING_SIZE, 4096);
//         static constexpr std::size_t BLOB_SIZE = (SHM_INBOX_SIZE > BLOB_OFFSET) ? (SHM_INBOX_SIZE - BLOB_OFFSET) : 0;
//     };

//     static_assert(SHM_DESCRIPTOR_RING_SIZE % sizeof(InboxDescriptor) == 0);
//     static_assert(InboxLayout::BLOB_SIZE > 0);
//     static_assert(InboxLayout::BLOB_OFFSET < SHM_INBOX_SIZE);
//     static_assert(InboxLayout::DESC_COUNT == 131072);
// }