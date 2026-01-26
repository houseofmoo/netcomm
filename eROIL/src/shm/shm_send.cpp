#include "shm_send.h"
#include <thread>
#include <cstring>
#include <memory>
#include <eROIL/print.h>

namespace eroil::shm {
    ShmSend::ShmSend(NodeId dst_id) : m_dst_id(dst_id), m_shm(dst_id, SHM_BLOCK_SIZE), m_event(dst_id) {}
    ShmSend::~ShmSend() = default;

    bool ShmSend::open() {
        // senders only ever open a destination nodes shared memory block
        // never create it. try a few times before giving up
        for (int i = 0; i < 50; ++i) {
            auto oerr = m_shm.open();
            if (oerr == shm::ShmErr::None) {
                return true;
            }
            std::this_thread::yield();
        }

        return false;
    }

    void ShmSend::close() {
        m_shm.close();
        m_event.close();
    }

    ShmSendErr ShmSend::write_data(ShmSendPayload send) {
        // if not initialized, this message is lost (consumer is re-initing)
        auto* hdr  = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            ERR_PRINT(__func__, " shm block not initialized for nodeid=", m_dst_id);
            return ShmSendErr::BlockNotInitialized;
        }

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        uint64_t gen = meta->generation.load(std::memory_order_acquire);

        // attempt to allocate space in shared memory
        const size_t size = send.buf_size + sizeof(RecordHeader);
        const size_t reserved = align_up(size, 8);

        // this is a hard error condition that should never occur
        if (reserved > ShmLayout::DATA_BLOCK_SIZE) {
            ERR_PRINT(__func__, " tried to reserve more than allowed, reserved=", reserved, 
                      ", allowed=", ShmLayout::DATA_BLOCK_SIZE, ", to nodeid=", m_dst_id);
            return ShmSendErr::SizeTooLarge;
        }

        bool reserved_success = false;
        size_t head = meta->head_bytes.load(std::memory_order_acquire);
        //LOG("START: head=", head);
        
        for (int tries = 0; tries < 100; ++tries) {
            const size_t tail = meta->tail_bytes.load(std::memory_order_acquire);
            if (head < tail) return ShmSendErr::AllocatorCorrupted;
            const size_t used = head - tail;
            
            // consumer has not freed enough space for this message
            if (used + reserved > ShmLayout::DATA_BLOCK_SIZE) {
                ERR_PRINT(__func__, " not enough space available size=", reserved,
                         " to nodeid=", m_dst_id, " CONSUMER IS TOO SLOW!");
                return ShmSendErr::NotEnoughSpace;
            }

            const size_t head_offset = head % ShmLayout::DATA_BLOCK_SIZE;
            if (head_offset > ShmLayout::DATA_USABLE_LIMIT) { // should never occur
                ERR_PRINT(__func__, " head pushed out of usable zone, allocator corrupted");
                return ShmSendErr::AllocatorCorrupted;
            }

            // current position + allocation would prevent a wrap header from being written, wrap now
            if (head_offset + reserved > ShmLayout::DATA_USABLE_LIMIT) {
                const size_t space_til_wrap = ShmLayout::DATA_BLOCK_SIZE - head_offset;
                const size_t new_head = head + space_til_wrap;

                // compare exchange success means we allocatedor 
                // failure writes the actual head value back into head
                if (meta->head_bytes.compare_exchange_weak(head, new_head,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_relaxed)) {
                    // we've allocated pad bytes, write the wrap record header
                    auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(head));
                    rec_hdr->state.store(WRITING, std::memory_order_relaxed);
                    rec_hdr->magic = MAGIC_NUM;
                    rec_hdr->total_size = space_til_wrap;
                    rec_hdr->payload_size = 0;
                    rec_hdr->flags = 0;
                    rec_hdr->user_seq = 0;
                    rec_hdr->epoch = gen;
                    rec_hdr->label = 0;
                    rec_hdr->source_id = 0;
                    rec_hdr->state.store(WRAP, std::memory_order_release);
                    head = new_head;
                }
                continue;
            }

            const size_t new_head = head + reserved;
            if (meta->head_bytes.compare_exchange_weak(head, new_head,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
                // successfully allocated space for our record
                reserved_success = true;
                break;
            }
            // someone else allocated before us, we need to try again
        }

        // was never able to allocate space
        if (!reserved_success) {
            ERR_PRINT(
                __func__, 
                " could not allocate space for label=", send.label, " to nodeid=", m_dst_id
            );
            return ShmSendErr::CouldNotAllocate;
        }
        
        // if a re-init happened while we were allocating, abandon
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY ||
            meta->generation.load(std::memory_order_acquire) != gen) {
            return ShmSendErr::BlockReinitialized;
        }
        
        // set writing and fill in header
        auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(head));
        rec_hdr->state.store(WRITING, std::memory_order_relaxed);
        rec_hdr->magic = MAGIC_NUM;
        rec_hdr->total_size = reserved;
        rec_hdr->payload_size = send.buf_size;
        rec_hdr->flags = 0;
        rec_hdr->user_seq = send.user_seq;
        rec_hdr->epoch = gen;
        rec_hdr->label = send.label;
        rec_hdr->source_id = send.source_id;

        // copy data immediately after record header
        m_shm.write(send.buf.get(), send.buf_size, get_data_offset(head));

        // publish this record is ready
        rec_hdr->state.store(COMMITTED, std::memory_order_release);

        // increment publish count (debugging only)
        meta->published_count.fetch_add(1, std::memory_order_relaxed);

        // notify
        m_event.post();

        // LOG("END: tail=", meta->tail_bytes.load(std::memory_order_acquire));
        // LOG("END: head=", meta->head_bytes.load(std::memory_order_acquire));
        // LOG("END: allocated=", reserved);

        return ShmSendErr::None;
    }
}