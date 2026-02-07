#pragma once
#include <memory>
#include "shm.h"
#include "events/named_semaphore.h"
#include "types/const_types.h"
#include "shm_header.h"
#include "macros.h"

namespace eroil::shm {
    enum class ShmRecvErr {
        None,               // success
        BlockNotInitialized,// hard error, we should only be running when the block is initialized
        NoRecords,          // wait for event
        NotYetPublished,    // try again later
        
        TailCorruption,     // re-init
        BlockCorrupted,     // re-init
        UnknownError        // re-init
    };

    struct ShmRecvResult {
        ShmRecvErr err = ShmRecvErr::None;
        NodeId source_id = INVALID_NODE;
        Label label = INVALID_LABEL;
        uint32_t user_seq = 0;
        size_t buf_size = 0;
        std::unique_ptr<std::byte[]> buf = nullptr;

        explicit ShmRecvResult() = default;
        explicit ShmRecvResult(ShmRecvErr error) : err(error) {}
    };

    class ShmRecv {
        private:
            NodeId m_id;
            Shm m_shm;
            evt::NamedSemaphore m_event;
            ShmHeader* m_shm_hdr = nullptr;
            ShmMetaData* m_shm_meta = nullptr;

        public:
            ShmRecv(NodeId id);
            ~ShmRecv();

            EROIL_NO_COPY(ShmRecv)
            EROIL_NO_MOVE(ShmRecv)

            bool create_or_open();
            void close();
            bool init_as_new();
            bool reinit();
            evt::NamedSemErr wait();
            ShmRecvResult recv();
            void flush_backlog();
    };
}