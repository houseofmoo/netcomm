#include "shm_recv.h"
#include <cstring>
#include <memory>
#include <eROIL/print.h>

namespace eroil::shm {
    ShmRecv::ShmRecv(NodeId id) : m_id(id), m_shm(id, SHM_BLOCK_SIZE), m_event(id) {}
    ShmRecv::~ShmRecv() = default;

    bool ShmRecv::create_or_open() {
        if (!validate_layout(m_shm.total_size())) {
            ERR_PRINT("shm recv block size does not match expected size");
            ERR_PRINT("    expected=", SHM_BLOCK_SIZE, ", actual=", m_shm.total_size());
            return false;
        }

        // everyone opens their own recv shared memory block
        // 3 possibilities:
        //      block doesnt exist -> create it and set it up
        //      block exists but invalid -> reset it
        //      block exists but valid -> reset it
        shm::ShmResult create_result = m_shm.create();
        switch (create_result.code) {
            case ShmErr::None: { return init_as_new(); }
            case ShmErr::AlreadyExists: { break; } // try to open it below
            case ShmErr::DoubleOpen:    // fallthrough
            case ShmErr::InvalidName:   // fallthrough
            case ShmErr::UnknownError:  // fallthrough
            case ShmErr::FileMapFailed: { 
                ERR_PRINT(" shm create err=", create_result.code_to_string()); 
                return false; 
            }
            default: {
                ERR_PRINT("shm create unknown error=", create_result.code_to_string());
                return false;
            }
        }

        // block existed, opening it instead
        shm::ShmResult open_result = m_shm.open();
        switch (open_result.code) {
            case ShmErr::None: { return reinit(); }
            case ShmErr::DoubleOpen:    // fallthrough
            case ShmErr::InvalidName:   // fallthrough
            case ShmErr::DoesNotExist:  // fallthrough
            case ShmErr::UnknownError:  // fallthrough    
            case ShmErr::SizeMismatch:  // fallthrough
            case ShmErr::FileMapFailed: {
                ERR_PRINT(" shm open err=", open_result.code_to_string()); 
                return false; 
            }
            default: {
                ERR_PRINT("shm open unknown error=", open_result.code_to_string());
                return false;
            }
        }

        // failed to open
        return false;
    }

    void ShmRecv::close() {
        m_shm.close();
        m_event.close();
    }

    bool ShmRecv::init_as_new() {
        // announce this block is being initialized
        auto* hdr = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr == nullptr) {
            ERR_PRINT("shm header offset was invalid, unable to init new recv shm block");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return false;
        }
        hdr->state.store(SHM_INITING, std::memory_order_relaxed);
        hdr->magic = MAGIC_NUM;
        hdr->version = VERSION;
        hdr->total_size = m_shm.total_size();

        // zero data block - probably redundant but just to be sure
        m_shm.memset(sizeof(ShmHeader), 0, m_shm.total_size() - sizeof(ShmHeader));
        
        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        if (meta == nullptr) {
            ERR_PRINT("shm meta offset was invalid, unable to init new recv shm block");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return false;
        }

        meta->node_id = m_id;
        meta->data_block_size = ShmLayout::DATA_BLOCK_SIZE;
        meta->generation.store(1, std::memory_order_relaxed);
        meta->head_bytes.store(0, std::memory_order_relaxed);
        meta->tail_bytes.store(0, std::memory_order_relaxed);
        meta->published_count.store(0, std::memory_order_relaxed);
        
        // announce this is ready for use
        hdr->state.store(SHM_READY, std::memory_order_release);
        return true;
    }

    bool ShmRecv::reinit() {
        auto* hdr = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr == nullptr) {
            ERR_PRINT("shm header offset was invalid, unable to re-init recv shm block");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return false;
        }
        
        // if we see "SHM_INITING" in our own recv block, something weird happend
        // either we died mid init/re-init or the shared memory block was zero'd out
        // either when opening the block (windows can do this) or something we did ourselves. 
        // not a show stopper since we're going to re-write all data to the block
        // if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
        //     ERR_PRINT("found existing shm recv block that was in INIT state, weird but continuing");
        //     ERR_PRINT("   state=", shm_state);
        // }
 
        // if header is mangled, assume block needs to be treated as new
        hdr->state.store(SHM_INITING, std::memory_order_release);
        if (hdr->magic != MAGIC_NUM ||
            hdr->version != VERSION ||
            hdr->total_size != SHM_BLOCK_SIZE) {
            return init_as_new();
        }

        // reset meta data and bump generation
        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        if (meta == nullptr) { // if this happens someone changed something and broke everything
            ERR_PRINT("shm meta offset was invalid, unable to re-init recv shm block");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return false;
        }

        meta->node_id = m_id;
        meta->data_block_size = ShmLayout::DATA_BLOCK_SIZE;
        meta->generation.fetch_add(1, std::memory_order_relaxed);
        meta->head_bytes.store(0, std::memory_order_relaxed);
        meta->tail_bytes.store(0, std::memory_order_relaxed);
        meta->published_count.store(0, std::memory_order_relaxed);
        
        // announce this is ready for use
        hdr->state.store(SHM_READY, std::memory_order_release);
        return true;
    }

    evt::NamedSemResult ShmRecv::wait() {
        return m_event.wait();
    }

    ShmRecvData ShmRecv::recv(std::byte* recv_buf, size_t max_size) {
        auto* hdr  = m_shm.map_to_type<ShmHeader>(ShmLayout::HDR_OFFSET);
        if (hdr == nullptr) {
            ERR_PRINT("shm recv header pointer offset invalid");
            ERR_PRINT("    offset=", ShmLayout::HDR_OFFSET);
            return ShmRecvData{ShmRecvErr::BlockCorrupted};
        }

        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            return ShmRecvData{ShmRecvErr::BlockNotInitialized};
        }

        auto* meta = m_shm.map_to_type<ShmMetaData>(ShmLayout::META_DATA_OFFSET);
        if (meta == nullptr) {
            ERR_PRINT("shm recv meta pointer offset invalid");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return ShmRecvData{ShmRecvErr::BlockCorrupted};
        }

        uint64_t gen = meta->generation.load(std::memory_order_acquire);
        uint64_t tail = meta->tail_bytes.load(std::memory_order_acquire);
        uint64_t head = meta->head_bytes.load(std::memory_order_acquire);

        if (head == tail) return ShmRecvData{ShmRecvErr::NoRecords};
        if (head < tail) {
            ERR_PRINT("tail advanced passed head, requires re-init");
            return ShmRecvData{ShmRecvErr::TailCorruption};
        }

        // find next COMMITTED record, moving passed WRAP records we find
        bool found = false;
        while (head > tail && !found) {
            auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(tail));
            const uint32_t state = rec_hdr->state.load(std::memory_order_acquire);
            
            if (state == WRITING) {
                return ShmRecvData{ShmRecvErr::NotYetPublished};
            }

            if (rec_hdr->magic != MAGIC_NUM) {
                return ShmRecvData{ShmRecvErr::BlockCorrupted};
            }

            // we cannot trust messages, flush the backlog and continue
            if (rec_hdr->epoch != gen) {
                ERR_PRINT("flushing backlog due to invalid record generation");
                flush_backlog();
                return ShmRecvData{ShmRecvErr::NoRecords};
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
                       (rec_hdr->payload_size != 0)) return ShmRecvData{ShmRecvErr::BlockCorrupted};

                    // move tail_bytes passed the wrap record for next iteration
                    // update head incase someone allocated while we did this
                    const uint64_t new_tail = tail + static_cast<uint64_t>(total_size);
                    meta->tail_bytes.store(new_tail, std::memory_order_release);
                    tail = new_tail;
                    head = meta->head_bytes.load(std::memory_order_acquire);
                    continue;
                }

                default: { // this should NEVER happen
                    ERR_PRINT("got unknown record hdr state=", state);
                    return ShmRecvData{ShmRecvErr::UnknownError};
                }
            }
        }

        // nothing to read
        if (!found) return ShmRecvData{ShmRecvErr::NoRecords};
        
        // read header and data
        auto* rec_hdr = m_shm.map_to_type<RecordHeader>(get_header_offset(tail));
        if (rec_hdr == nullptr) {
            ERR_PRINT("rec_hdr was null, tail offset invalid, offset=", get_header_offset(tail));
            return ShmRecvData{ShmRecvErr::BlockCorrupted};
        }

        // validate best we can
        const size_t total_size = rec_hdr->total_size;
        if (total_size < sizeof(RecordHeader) ||
           ((total_size & 7u) != 0) ||
           (total_size > ShmLayout::DATA_BLOCK_SIZE) ||
           (rec_hdr->payload_size == 0))  { return ShmRecvData{ShmRecvErr::BlockCorrupted}; }

        // if label is too large to fit in the provided buffer, return error
        if (rec_hdr->payload_size > max_size) {
            ERR_PRINT("recv buffer too small for next record, required size=", rec_hdr->payload_size);
            return ShmRecvData{ShmRecvErr::LabelTooLarge};
        }

        // copy data to provided buffer
        ShmRecvData out;
        out.source_id = rec_hdr->source_id;
        out.label = rec_hdr->label;
        out.user_seq = rec_hdr->user_seq;
        out.buf_size = rec_hdr->payload_size;
        out.recv_buf = recv_buf;
        shm::ShmResult read_result = m_shm.read(out.recv_buf, out.buf_size, get_data_offset(tail));
        if (!read_result.ok()) {
            ERR_PRINT("shm read error=", read_result.code_to_string());
        }

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
        if (meta == nullptr) {
            ERR_PRINT("shm recv meta pointer offset invalid");
            ERR_PRINT("    offset=", ShmLayout::META_DATA_OFFSET);
            return;
        }
        
        const uint64_t head = meta->head_bytes.load(std::memory_order_acquire);
        meta->tail_bytes.store(head, std::memory_order_release);
        meta->published_count.store(0, std::memory_order_relaxed);
        LOG("flushed shm recv backlog");
    }
}