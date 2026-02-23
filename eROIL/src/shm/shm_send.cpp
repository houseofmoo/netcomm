#include "shm_send.h"
#include <thread>
#include <cstring>
#include <memory>
#include <eROIL/print.h>

namespace eroil::shm {
    ShmSend::ShmSend(NodeId dst_id) : m_dst_id(dst_id), m_shm(dst_id, SHM_BLOCK_SIZE), m_event(dst_id) {}
    ShmSend::~ShmSend() = default;

    bool ShmSend::open() {
        if (!validate_layout(m_shm.total_size())) {
            ERR_PRINT("shm send block size does not match expected size");
            ERR_PRINT("    expected=", SHM_BLOCK_SIZE, ", actual=", m_shm.total_size());
            return false;
        }

        // senders only ever open a destination nodes shared memory block
        // never create it. try a few times before giving up
        for (int i = 0; i < 50; ++i) {
            shm::ShmResult open_result = m_shm.open();
            if (open_result.ok()) {
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

    ShmSendResult ShmSend::send(const NodeId id, 
                                const Label label, 
                                const uint32_t seq, 
                                const size_t buf_size, 
                                const std::byte* buf) {

        auto* hdr = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr == nullptr) {
            ERR_PRINT("shm send header pointer offset invalid");
            ERR_PRINT("    shm total size=", m_shm.total_size());
            ERR_PRINT("    offset=", ShmLayout::HDR_OFFSET, " dstid=", m_dst_id, " label=", label);
            return { ShmSendErr::InvalidOffset, ShmSendOp::Send };
        }
        
        // if not initialized, this message is lost (consumer is re-initing)
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            ERR_PRINT("shm block not initialized for nodeid=", m_dst_id);
            return { ShmSendErr::BlockNotInitialized, ShmSendOp::Send };
        }

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        if (meta == nullptr) {
            ERR_PRINT("shm send meta pointer offset invalid");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return { ShmSendErr::InvalidOffset, ShmSendOp::Send };
        }

        uint64_t gen = meta->generation.load(std::memory_order_acquire);

        // how much space will we need for this data send
        const size_t reserved = align_up(buf_size + sizeof(RecordHeader), 8);
        
        // this is a hard error condition that should never occur
        if (reserved > ShmLayout::DATA_BLOCK_SIZE) {
            ERR_PRINT("tried to reserve more than allowed, reserved=", reserved, 
                      " allowed=", ShmLayout::DATA_BLOCK_SIZE, ", to nodeid=", m_dst_id);
            return { ShmSendErr::SizeTooLarge, ShmSendOp::Send };
        }

        bool reserved_success = false;
        uint64_t head = meta->head_bytes.load(std::memory_order_acquire);
        
        for (int tries = 0; tries < 100; ++tries) {
            const uint64_t tail = meta->tail_bytes.load(std::memory_order_acquire);
            if (head < tail) {
                // assumption: consumer moved tail after we loaded head
                // if the head/tail are actually corrupted, consumer should fix it
                head = meta->head_bytes.load(std::memory_order_acquire);
                continue;
            }
            
            // consumer has not freed enough space for this message
            const uint64_t used = head - tail;
            if (used + reserved > ShmLayout::DATA_BLOCK_SIZE) {
                ERR_PRINT("not enough space available size=", reserved,
                          " to nodeid=", m_dst_id, " CONSUMER IS TOO SLOW!");
                return { ShmSendErr::NotEnoughSpace, ShmSendOp::Send };
            }

            // logic error: some writer wrote a data record instead of wrap record and broke things
            const size_t head_offset = head % ShmLayout::DATA_BLOCK_SIZE;
            if (head_offset > ShmLayout::DATA_USABLE_LIMIT) {
                ERR_PRINT("head pushed out of usable zone, allocator corrupted");
                return { ShmSendErr::AllocatorCorrupted, ShmSendOp::Send };
            }

            // current position + allocation would prevent a wrap header from being written, wrap now
            if (head_offset + reserved > ShmLayout::DATA_USABLE_LIMIT) {
                const size_t space_til_wrap = ShmLayout::DATA_BLOCK_SIZE - head_offset;
                const uint64_t new_head = head + static_cast<uint64_t>(space_til_wrap);

                // compare exchange success means we allocated for wrap record
                // failure means someone else handled it
                if (meta->head_bytes.compare_exchange_weak(head, new_head,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_relaxed)) {
                    auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(head));
                    if (rec_hdr == nullptr) { // if this happens someone changed something and broke everything
                        ERR_PRINT("rec_hdr ptr null and could not allocate memory, head offset was invalid");
                        ERR_PRINT("    offset=", get_header_offset(head));
                        continue;
                    }

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

            // compare exchange success means we allocated for data record
            // failure means someone else allocated before us and head contains the updated head
            const uint64_t new_head = head + static_cast<uint64_t>(reserved);
            if (meta->head_bytes.compare_exchange_weak(head, new_head,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
                reserved_success = true;
                break;
            }
        }

        // was never able to allocate space
        if (!reserved_success) {
            ERR_PRINT(" could not allocate space for label=", label, " to nodeid=", m_dst_id);
            return { ShmSendErr::CouldNotAllocate, ShmSendOp::Send };
        }
        
        // if a re-init happened while we were allocating, abandon
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY ||
            meta->generation.load(std::memory_order_acquire) != gen) {
            return { ShmSendErr::BlockReinitialized, ShmSendOp::Send };
        }
        
        // set writing and fill in header
        auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(head));
        if (rec_hdr == nullptr) { // if this happens someone changed something and broke everything
            ERR_PRINT("rec_hdr ptr null, head offset was invalid");
            ERR_PRINT("    offset=", get_header_offset(head));
            return { ShmSendErr::InvalidOffset, ShmSendOp::Send };
        }

        rec_hdr->state.store(WRITING, std::memory_order_relaxed);
        rec_hdr->magic = MAGIC_NUM;
        rec_hdr->total_size = reserved;
        rec_hdr->payload_size = buf_size;
        rec_hdr->flags = 0;
        rec_hdr->user_seq = seq;
        rec_hdr->epoch = gen;
        rec_hdr->label = label;
        rec_hdr->source_id = id;

        // copy data immediately after record header
        shm::ShmResult write_result = m_shm.write(buf, buf_size, get_data_offset(head));
        if (!write_result.ok()) {
            ERR_PRINT("shm write failed, err=", write_result.code_to_string());
        }

        // publish this record is ready
        rec_hdr->state.store(COMMITTED, std::memory_order_release);

        // increment publish count (debugging only)
        meta->published_count.fetch_add(1, std::memory_order_relaxed);

        // notify
        evt::NamedSemResult post_result = m_event.post();
        if (!post_result.ok()) {
            ERR_PRINT("shm send notification failed, err=", post_result.code_to_string());
        }

        return { ShmSendErr::None, ShmSendOp::Send };
    }
}