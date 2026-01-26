#pragma once
#include "shm.h"
#include "events/named_event.h"
#include "types/types.h"
#include "shm_header.h"

namespace eroil::shm {
    enum class ShmSendErr {
        None,               // success
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
            evt::NamedEvent m_event;
        
        public:
            ShmSend(NodeId dst_id);
            ~ShmSend();

            bool open();
            void close();
            ShmSendErr write_data(ShmSendPayload send);
    };
}