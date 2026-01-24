// #include "inbox.h"
// #include "shm/shm_header.h"
// #include "inbox_header.h"
// #include "inbox_layout.h"
// #include "assertion.h"

// namespace eroil::ibx {
//     // need to consider alot of this code
//     // will have to work the other way for sending
//     // so maybe instead of inbox we have "recv shm" and "send shm" again like before
//     // 

//     Inbox::Inbox(NodeId id) : 
//         m_id(id), 
//         m_shm(m_id, SHM_INBOX_SIZE), // 128MB total size
//         m_event(id) {}

//     Inbox::~Inbox() = default;

//     bool Inbox::open() {
//         auto cerr = m_shm.create();
//         if (cerr == shm::ShmErr::None) {
//             // we created it, lets set it up
//             m_shm.write_header_init();

//             // zero out entire shared memory block other than shm header
//             m_shm.zero_data_block();
            
//             // write inbox header
//             InboxHeader inbox_hdr;
//             inbox_hdr.magic = MAGIC_NUM;
//             inbox_hdr.version = VERSION;
//             inbox_hdr.generation = 1; // TODO: some random 64 bit uint
//             inbox_hdr.node_id = m_id;
//             inbox_hdr.descriptor_region_offset = InboxLayout::DESC_OFFSET;
//             inbox_hdr.descriptor_region_size = SHM_DESCRIPTOR_RING_SIZE;
//             inbox_hdr.descriptor_capacity = InboxLayout::DESC_COUNT;
//             inbox_hdr.descriptor_mask = InboxLayout::DESC_COUNT - 1;
//             inbox_hdr.blob_region_offset = InboxLayout::BLOB_OFFSET;
//             inbox_hdr.blob_region_size   = InboxLayout::BLOB_SIZE;
//             inbox_hdr.write_ticket.store(0, std::memory_order_relaxed);
//             inbox_hdr.read_ticket.store(0, std::memory_order_relaxed);
//             inbox_hdr.blob_head.store(0, std::memory_order_relaxed);
//             inbox_hdr.blob_tail.store(0, std::memory_order_relaxed);
//             m_shm.write(&inbox_hdr, sizeof(inbox_hdr), InboxLayout::INBOX_HDR_OFFSET);
            
//             // write descriptors
//             InboxDescriptor desc;
//             for (int i = 0; i < InboxLayout::DESC_COUNT; ++i) {
//                 desc.slot_seq.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
//                 size_t offset = InboxLayout::DESC_OFFSET + (i * sizeof(desc));
//                 m_shm.write(&desc, sizeof(desc, InboxLayout::DESC_OFFSET), offset);
//             }

//             // announce this is ready for use
//             m_shm.write_header_ready();
//             return true;
//         }

//         // otherwise it already exists and we validate if its been set up
//         auto oerr = m_shm.open();
//         if (oerr == shm::ShmErr::None) {
//             // TODO: validate

//             return true;
//         }

//         return false;
//     }
// }