#include "shm_recv.h"
#include <cstring>
#include <memory>
#include <eROIL/print.h>

namespace eroil::shm {
    ShmRecv::ShmRecv(NodeId id) : m_id(id), m_shm(id, SHM_BLOCK_SIZE), m_event(id) {}
    ShmRecv::~ShmRecv() = default;

    bool ShmRecv::create_or_open() {
        // everyone opens their own recv shared memory block
        // 3 possibilities:
        //      block doesnt exist -> create it and set it up
        //      block exists but invalid -> reset it
        //      block exists but valid -> still reset it (we crashed likely)
        auto cerr = m_shm.create();
        if (cerr == shm::ShmErr::None) {
            init_as_new();
            return true;
        }

        auto oerr = m_shm.open();
        if (oerr == shm::ShmErr::None) {
            reinit();
            return true;
        }

        // failed
        return false;
    }

    void ShmRecv::close() {
        m_shm.close();
        m_event.close();
    }

    void ShmRecv::init_as_new() {
        // announce this block is being initialized
        auto* hdr = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        hdr->state.store(SHM_INITING, std::memory_order_relaxed);
        hdr->magic = MAGIC_NUM;
        hdr->version = VERSION;
        hdr->total_size = SHM_BLOCK_SIZE;

        // zero data block - probably redundant but just to be sure
        m_shm.memset(sizeof(ShmHeader), 0, SHM_BLOCK_SIZE - sizeof(ShmHeader));
        
        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        meta->node_id = m_id;
        meta->data_block_size = ShmLayout::DATA_BLOCK_SIZE;
        meta->generation.store(1, std::memory_order_relaxed);
        meta->head_bytes.store(0, std::memory_order_relaxed);
        meta->tail_bytes.store(0, std::memory_order_relaxed);
        meta->published_count.store(0, std::memory_order_relaxed);
        
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
            ERR_PRINT("found existing shm recv block that was in INIT state, weird but continuing");
        }
 
        // if header is mangled, assume block needs to be treated as new
        hdr->state.store(SHM_INITING, std::memory_order_release);
        if (hdr->magic != MAGIC_NUM ||
            hdr->version != VERSION ||
            hdr->total_size != SHM_BLOCK_SIZE) {
            init_as_new();
            return;
        }

        // reset meta data and bump generation
        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        meta->node_id = m_id;
        meta->data_block_size = ShmLayout::DATA_BLOCK_SIZE;
        meta->generation.fetch_add(1, std::memory_order_relaxed);
        meta->head_bytes.store(0, std::memory_order_relaxed);
        meta->tail_bytes.store(0, std::memory_order_relaxed);
        meta->published_count.store(0, std::memory_order_relaxed);

        // zero out block
        m_shm.memset(ShmLayout::DATA_BLOCK_OFFSET, 0, ShmLayout::DATA_BLOCK_SIZE);
        
        // announce this is ready for use
        hdr->state.store(SHM_READY, std::memory_order_release);
    }

    evt::NamedEventErr ShmRecv::wait() {
        return m_event.wait();
    }

    ShmRecvResult ShmRecv::recv() {
        auto* hdr  = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            return ShmRecvResult{ShmRecvErr::BlockNotInitialized};
        }

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        uint64_t gen = meta->generation.load(std::memory_order_acquire);
        uint64_t tail = meta->tail_bytes.load(std::memory_order_acquire);
        uint64_t head = meta->head_bytes.load(std::memory_order_acquire);

        if (head == tail) return ShmRecvResult{ShmRecvErr::NoRecords};
        if (head < tail) {
            ERR_PRINT(" tail advanced passed head, requires re-init");
            return ShmRecvResult{ShmRecvErr::TailCorruption};
        }

        // find next COMMITTED record, moving passed WRAP records we find
        bool found = false;
        while (head > tail && !found) {
            auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(tail));
            const auto state = rec_hdr->state.load(std::memory_order_acquire);
            
            if (state == WRITING) {
                return ShmRecvResult{ShmRecvErr::NotYetPublished};
            }

            if (rec_hdr->magic != MAGIC_NUM) {
                return ShmRecvResult{ShmRecvErr::BlockCorrupted};
            }

            // we cannot trust messages, flush the backlog and continue
            if (rec_hdr->epoch != gen) {
                ERR_PRINT("flushing backlog due to invalid record generation");
                flush_backlog();
                return ShmRecvResult{ShmRecvErr::NoRecords};
            }

            switch (state) {
                case COMMITTED: {
                    found = true;
                    break;
                }

                case WRAP: {
                    // validate best we can
                    const size_t total_size = rec_hdr->total_size;
                    if (total_size < sizeof(RecordHeader) ||
                       ((total_size & 7u) != 0) ||
                       (total_size > ShmLayout::DATA_BLOCK_SIZE) ||
                       (rec_hdr->payload_size != 0)) return ShmRecvResult{ShmRecvErr::BlockCorrupted};

                    // move tail_bytes passed the wrap record for next iteration
                    // update head incase someone allocated while we did this
                    const uint64_t new_tail = tail + static_cast<uint64_t>(total_size);
                    meta->tail_bytes.store(new_tail, std::memory_order_release);
                    tail = new_tail;
                    head = meta->head_bytes.load(std::memory_order_acquire);
                    continue;
                }

                default: { // this should NEVER happen
                    ERR_PRINT(" got unknown record hdr state=", state);
                    return ShmRecvResult{ShmRecvErr::UnknownError};
                }
            }
        }

        // nothing to read
        if (!found) return ShmRecvResult{ShmRecvErr::NoRecords};
        
        // read header and data
        auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(tail));

        // validate best we can
        const size_t total_size = rec_hdr->total_size;
        if (total_size < sizeof(RecordHeader) ||
           ((total_size & 7u) != 0) ||
           (total_size > ShmLayout::DATA_BLOCK_SIZE) ||
           (rec_hdr->payload_size == 0)) return ShmRecvResult{ShmRecvErr::BlockCorrupted};

        ShmRecvResult out;
        out.source_id = rec_hdr->source_id;
        out.label = rec_hdr->label;
        out.user_seq = rec_hdr->user_seq;
        out.buf_size = rec_hdr->payload_size;
        out.buf = std::make_unique<std::byte[]>(rec_hdr->payload_size);
        m_shm.read(out.buf.get(), out.buf_size, get_data_offset(tail));

        // move tail_bytes passed the record we've just read
        const uint64_t new_tail = tail + static_cast<uint64_t>(total_size);
        meta->tail_bytes.store(new_tail, std::memory_order_release);

        // use publish count to keep an eye on the number of items published vs yet to consume (debugging only)
        meta->published_count.fetch_sub(1, std::memory_order_relaxed);

        return out;
    }

    void ShmRecv::flush_backlog() {
        // in the case where we have found a WRITING record or a record with an old generation
        // we're in a situation where we do not know if we can trust that record to allow us to continue
        // 
        // since we dont know what to trust, trust non of it. flush ALL old messages by advancing the 
        // tail to head, and continuing normally
        //
        // this should be an EXTREMELY rare case:
        //      - writer died mid-write before publishing "COMMITTED" message
        //      - writer was allowed to write even though generation changed after re-init
        //

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        const uint64_t head = meta->head_bytes.load(std::memory_order_acquire);
        meta->tail_bytes.store(head, std::memory_order_release);
        LOG("flushed shm recv backlog");
    }
}