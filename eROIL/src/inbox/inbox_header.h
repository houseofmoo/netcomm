// #pragma once
// #include <atomic>
// #include "types/types.h"

// namespace eroil::ibx {
//     // each shm block will be 128MB + sizeof(ShmHeader) + sizeof(InboxHeader) in size
//     // the block will be split into two parts, descriptor ring and data blob ring
//     // descriptor ring 8 MB
//     // data blob ring 120MB
//     // writes "claim" space in the blob ring via descriptors
//     struct alignas(64) InboxHeader {
//         uint32_t magic = 12345;
//         uint16_t version = 1;
//         uint64_t generation = 0;
//         uint16_t _pad = 0;
//         NodeId node_id = INVALID_NODE; // id of the node this belongs to

//         // descriptor ring
//         uint32_t descriptor_capacity = 0; // num slots in descriptor ring (pow of 2)
//         uint32_t descriptor_mask = 0;   // cap - 1
//         uint32_t descriptor_region_offset = 0;
//         uint32_t descriptor_region_size = 0;
//         alignas(64) std::atomic<uint64_t> write_ticket{0}; // next sequence to claim
//         alignas(64) std::atomic<uint64_t> read_ticket{0}; // next sequence consumer reads
//         alignas(64) std::atomic<uint64_t> published_count{0}; // for debugging

//         // blob ring
//         uint32_t blob_region_offset = 0;
//         uint32_t blob_region_size = 0;
//         alignas(64) std::atomic<uint32_t> blob_head{0}; // producer alloc head (offset into blob region)
//         alignas(64) std::atomic<uint32_t> blob_tail{0}; // consumer free tail
//     };
//     static_assert(sizeof(InboxHeader) % 64 == 0); // with alignment befores 384 bytes
//     static_assert(sizeof(InboxHeader) == 384); // just for myself to see if the size is changing

//     struct alignas(64) InboxDescriptor {
//         std::atomic<uint64_t> slot_seq{0};

//         // meta data
//         NodeId source_id = INVALID_NODE;
//         Label label = INVALID_LABEL;
//         uint32_t flags = 0;

//         // blobl info
//         uint32_t blob_offset = 0;
//         uint32_t blob_size = 0;

//         // recv
//         uint32_t recv_offset = 0;
//         uint32_t _pad = 0;

//         uint64_t user_seq = 0;
//     };
//     static_assert(sizeof(InboxDescriptor) == 64);
// }