#include "shm_io.h"
#include <cstring>
#include "shm_header.h"
#include <eROIL/print.h>

namespace eroil::shm {
    inline void publish_drop(ShmBlockDescriptor& slot, uint64_t ticket, NodeId id, Label label) {
        slot.source_id = id;
        slot.label = label;
        slot.flags = SHM_FLAG_DROPPED;
        slot.blob_offset = 0;
        slot.blob_size = 0;
        slot.user_seq = 0;
        slot.slot_seq.store(ticket + 1, std::memory_order_release);
    }

    ShmRecv::ShmRecv(NodeId id) : m_id(id), m_shm(id, SHM_BLOCK_SIZE), m_event(id) {}
    ShmRecv::~ShmRecv() = default;

    // everyone opens their own recv shared memory block
    // 3 possibilities:
    //      block doesnt exist -> create it and set it up
    //      block exists but invalid -> reset it
    //      block exists but valid -> still reset it (we crashed likely)
    bool ShmRecv::create_or_open() {
        auto cerr = m_shm.create();
        if (cerr == shm::ShmErr::None) {
            init_new();
            return true;
        }

        auto oerr = m_shm.open();
        if (oerr == shm::ShmErr::None) {
            // TODO: validate
            reinit();
            return true;
        }

        // failed
        return false;
    }

    void ShmRecv::init_new() {
        // announce this block is being initialized
        auto* hdr = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        hdr->state.store(SHM_INITING, std::memory_order_relaxed);
        hdr->magic = MAGIC_NUM;
        hdr->version = VERSION;
        hdr->total_size = static_cast<uint32_t>(SHM_BLOCK_SIZE);

        // zero data block - probably redundant but just to be sure
        m_shm.memset(sizeof(ShmHeader), 0, SHM_BLOCK_SIZE - sizeof(ShmHeader));
        
        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        meta->magic = MAGIC_NUM;
        meta->version = VERSION;
        meta->generation = 1; // TODO: some random 64 bit uint
        meta->node_id = m_id;
        meta->descriptor_capacity = ShmLayout::DESC_COUNT;
        meta->descriptor_mask = ShmLayout::DESC_COUNT - 1;
        meta->descriptor_region_offset = ShmLayout::DESC_OFFSET;
        meta->descriptor_region_size = SHM_DESCRIPTOR_RING_SIZE;
        meta->blob_region_offset = ShmLayout::BLOB_OFFSET;
        meta->blob_region_size   = ShmLayout::BLOB_SIZE;
        meta->write_ticket.store(0, std::memory_order_relaxed);
        meta->read_ticket.store(0, std::memory_order_relaxed);
        meta->published_count.store(0, std::memory_order_relaxed);
        meta->blob_head_bytes.store(0, std::memory_order_relaxed);
        meta->blob_tail_bytes.store(0, std::memory_order_relaxed);
        
        // reset descriptor ring
        auto* ring = m_shm.map_to_type<ShmBlockDescriptor>(ShmLayout::DESC_OFFSET);
        for (size_t i = 0; i < ShmLayout::DESC_COUNT; ++i) {
            ring[i].slot_seq.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }

        // announce this is ready for use
        hdr->state.store(SHM_READY, std::memory_order_release);
    }

    void ShmRecv::reinit() {
        // if we see "SHM_INITING" in our own recv block, something weird happend
        // since we should be the only ones that set that to SHM_INITING.
        // likely we crashed and are restarting, but in any case that is an error 
        // but not a show stopper, note it and continue
        auto* hdr = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            ERR_PRINT("found existing shm recv block that was in INIT state, reconciling");
        }
 
        // if header is mangled, assume block needs to be treated as new
        hdr->state.store(SHM_INITING, std::memory_order_release);
        if (hdr->magic != MAGIC_NUM ||
            hdr->version != VERSION ||
            hdr->total_size != static_cast<uint32_t>(SHM_BLOCK_SIZE)) {
            init_new();
            return;
        }

        // reset meta data and bump generation
        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        meta->magic = MAGIC_NUM;
        meta->version = VERSION;
        meta->generation += 1;
        meta->node_id = m_id;
        meta->descriptor_capacity = ShmLayout::DESC_COUNT;
        meta->descriptor_mask = ShmLayout::DESC_COUNT - 1;
        meta->descriptor_region_offset = ShmLayout::DESC_OFFSET;
        meta->descriptor_region_size = SHM_DESCRIPTOR_RING_SIZE;
        meta->blob_region_offset = ShmLayout::BLOB_OFFSET;
        meta->blob_region_size   = ShmLayout::BLOB_SIZE;
        meta->write_ticket.store(0, std::memory_order_relaxed);
        meta->read_ticket.store(0, std::memory_order_relaxed);
        meta->published_count.store(0, std::memory_order_relaxed);
        meta->blob_head_bytes.store(0, std::memory_order_relaxed);
        meta->blob_tail_bytes.store(0, std::memory_order_relaxed);

        // zero out descriptor ring/blob dataring
        m_shm.memset(ShmLayout::DESC_OFFSET, 0, SHM_BLOCK_SIZE - ShmLayout::DESC_OFFSET);

        // reset descriptor ring
        auto* ring = m_shm.map_to_type<ShmBlockDescriptor>(ShmLayout::DESC_OFFSET);
        for (size_t i = 0; i < ShmLayout::DESC_COUNT; ++i) {
            ring[i].slot_seq.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }
        
        // announce this is ready for use
        hdr->state.store(SHM_READY, std::memory_order_release);
    }

    ShmRecvErr ShmRecv::read_data(ShmRecvBlob& out) {
        auto* hdr  = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            return ShmRecvErr::BlockNotInitialized;
        }

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        auto* ring = m_shm.map_to_type<ShmBlockDescriptor>(ShmLayout::DESC_OFFSET);
        auto* blob = m_shm.map_to_type<std::byte>(ShmLayout::BLOB_OFFSET);

        const uint64_t ticket = meta->read_ticket.load(std::memory_order_relaxed);
        const size_t index = static_cast<size_t>(ticket & meta->descriptor_mask);
        ShmBlockDescriptor& desc_slot = ring[index];

        // check if this is published
        const uint64_t seq = desc_slot.slot_seq.load(std::memory_order_acquire);
        if (seq != ticket + 1) {
            return ShmRecvErr::NotYetPublished; // empty / not published yet
        }

        // if this is a dropped slot, just move on
        if ((desc_slot.flags & SHM_FLAG_DROPPED) != 0) {
            desc_slot.slot_seq.store(ticket + meta->descriptor_capacity, std::memory_order_release);
            meta->read_ticket.store(ticket + 1, std::memory_order_release);
            return ShmRecvErr::Dropped;
        }

        out.source_id = desc_slot.source_id;
        out.label = desc_slot.label;
        out.user_seq = desc_slot.user_seq;
        out.buf_size = desc_slot.blob_size;
        out.buf = std::make_unique<std::byte[]>(out.buf_size);
        std::memcpy(out.buf.get(), blob + desc_slot.blob_offset, desc_slot.blob_size);

        // move tail forward since we've consumed data from the tail
        const uint64_t reserved = align_up(static_cast<uint64_t>(desc_slot.blob_size), 8);
        meta->blob_tail_bytes.fetch_add(reserved, std::memory_order_release);

        // mark descriptor slot as free
        desc_slot.slot_seq.store(ticket + meta->descriptor_capacity, std::memory_order_release);

        // move to next read ticket
        meta->read_ticket.store(ticket + 1, std::memory_order_release);
        return ShmRecvErr::None;
    }

    ShmSendErr ShmSend::write_data(ShmSendBlob send_blob) {
        // if not initialized, this message is lost (assuming consumer died)
        auto* hdr  = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            ERR_PRINT(__func__, " shm block not initialized for nodeid=", m_dst_id);
            return ShmSendErr::BlockNotInitialized;
        }

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        auto* ring = m_shm.map_to_type<ShmBlockDescriptor>(ShmLayout::DESC_OFFSET);
        auto* blob = m_shm.map_to_type<std::byte>(ShmLayout::BLOB_OFFSET);

        // get our write ticket and check slot is available
        const uint64_t ticket = meta->write_ticket.fetch_add(1, std::memory_order_relaxed);
        const size_t index = static_cast<size_t>(ticket & meta->descriptor_mask);
        ShmBlockDescriptor& desc_slot = ring[index];

        // if no space in descriptors ring, this message is lost (assuming consumer died)
        const uint64_t seq = desc_slot.slot_seq.load(std::memory_order_acquire);
        if (seq != ticket) {
            ERR_PRINT(__func__, " descriptor ring full writing to nodeid=", m_dst_id);
            return ShmSendErr::DescriptorsRingFull;
        }

        // reserve blob space
        const uint64_t size = static_cast<uint64_t>(send_blob.buf_size);
        const uint64_t reserved = align_up(size, 8);

        uint64_t head = meta->blob_head_bytes.load(std::memory_order_relaxed);
        bool reserved_space = false;
        for (int tries = 0; tries < 1000; ++tries) {
            const uint64_t tail = meta->blob_tail_bytes.load(std::memory_order_acquire);
            const uint64_t used = head - tail;

            // this is a hard error condition that should never occur
            if (reserved > ShmLayout::BLOB_SIZE) {
                publish_drop(desc_slot, ticket, send_blob.source_id, send_blob.label);
                m_event.post();
                ERR_PRINT(
                    __func__, 
                    " tried to reserve more than total alowed data size, reserved=", reserved, 
                    " allowed=", ShmLayout::BLOB_SIZE,
                    " to nodeid=", m_dst_id
                );
                return ShmSendErr::SizeTooLarge;
            }
            
            // if not enough space in data blob ring, this message is lost (assuming consumer died)
            if (used + reserved > ShmLayout::BLOB_SIZE) {
                publish_drop(desc_slot, ticket, send_blob.source_id, send_blob.label);
                m_event.post();
                ERR_PRINT(
                    __func__, 
                    " tried to send data of size=", reserved,
                    " but not enough space to nodeid=", m_dst_id
                );
                return ShmSendErr::NoSpaceInDataBlob;
            }

            // if allocation would wrap, force early wrap around so we dont have data splits
            const uint64_t head_offset = head % ShmLayout::BLOB_SIZE;
            if (head_offset + reserved > ShmLayout::BLOB_SIZE) {
                const uint64_t pad = ShmLayout::BLOB_SIZE - head_offset;
                const uint64_t new_head = head + pad;
                if (meta->blob_head_bytes.compare_exchange_weak(head, new_head,
                                                                std::memory_order_acq_rel,
                                                                std::memory_order_relaxed)) {
                    head = new_head;
                }
                // head is new_head (back to start of data blob ring)
                // or head is some value set by someone that interrupted us
                // either way head points to the next place we need to attempt our allocation from
                continue; 
            }

            const uint64_t new_head = head + reserved;
            if (meta->blob_head_bytes.compare_exchange_weak(head, new_head,
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_relaxed)) {
                reserved_space = true;
                break; // 'head' is now start offset
            }
            // if we failed, someone changed the head and we need to reattempt 
            // reservering space
        }

        // tried lots of times but never got space in blob, publish this was dropped
        if (!reserved_space) {
            publish_drop(desc_slot, ticket, send_blob.source_id, send_blob.label);
            m_event.post();
            ERR_PRINT(
                __func__, 
                " tried to allocate space for send but was never able to for nodeid=", m_dst_id
            );
            return ShmSendErr::CouldNotAllocate;
        }

        // copy data blob into data blob ring
        const uint32_t blob_offset = static_cast<uint32_t>(head % ShmLayout::BLOB_SIZE);
        std::memcpy(blob + blob_offset, send_blob.buf.get(), send_blob.buf_size);

        // fill in descriptor
        desc_slot.source_id = send_blob.source_id;
        desc_slot.label = send_blob.label;
        desc_slot.flags = 0;
        desc_slot.blob_offset = blob_offset;
        desc_slot.blob_size = send_blob.buf_size;
        desc_slot.user_seq = send_blob.user_seq;

        // publish
        desc_slot.slot_seq.store(ticket + 1, std::memory_order_release);

        // notify
        m_event.post();
        return ShmSendErr::None;
    }
}