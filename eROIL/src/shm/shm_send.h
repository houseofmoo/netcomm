#pragma once
#include "shm.h"
#include "events/named_semaphore.h"
#include "types/const_types.h"
#include "shm_header.h"
#include "macros.h"

namespace eroil::shm {
    enum class ShmSendErr {
        None,               // success
        InvalidOffset,      // offset into header or meta invalid
        BlockNotInitialized,// try again or drop
        BlockReinitialized, // try again or drop
        NotEnoughSpace,     // try again or drop
        SizeTooLarge,       // hard error
        CouldNotAllocate,   // hard error
        AllocatorCorrupted  // fatal, maybe retry a few times
    };
 
    // shared memory we write labels to
    class ShmSend {
        private:
            NodeId m_dst_id;
            Shm m_shm;
            evt::NamedSemaphore m_event;

        public:
            ShmSend(NodeId dst_id);
            ~ShmSend();

            EROIL_NO_COPY(ShmSend)
            EROIL_NO_MOVE(ShmSend)

            bool open();
            void close();
            ShmSendErr send(const NodeId id, 
                            const Label label, 
                            const uint32_t seq, 
                            const size_t buf_size, 
                            const std::byte* buf);
    };
}