#pragma once
#include "shm.h"
#include "events/named_event.h"
#include "types/types.h"

namespace eroil::shm {
    enum class ShmRecvErr {
        None,
        BlockNotInitialized,
        NotYetPublished,
    };

    // shared memory we read labels from
    class ShmRecv {
        private:
            NodeId m_id;
            Shm m_shm;
            evt::NamedEvent m_event;

        public:
            ShmRecv(NodeId id);
            ~ShmRecv();

            // everyone opens their own recv shared memory block
            // 3 possibilities:
            //      block doesnt exist -> create it and set it up
            //      block exists but invalid -> reset it
            //      block exists but valid -> still reset it (we crashed likely)
            bool create_or_open();
            
        private:
            void init_new();
            void reinit();
            ShmRecvErr read_data(ShmRecvBlob& out);
    };

    enum class ShmSendErr {
        None,
        BlockNotInitialized,
        DescriptorsRingFull,
        SizeTooLarge,
        NoSpaceInDataBlob
    };

    // shared memory we write labels to
    class ShmSend {
        private:
            NodeId m_dst_id;
            Shm m_shm;
            evt::NamedEvent m_event;

            uint64_t m_block_generation;
        
        public:
            ShmSend(NodeId dst_id)  : m_dst_id(dst_id), m_shm(dst_id, SHM_BLOCK_SIZE), m_event(dst_id) {
                // read block generation and set it so we can check when writing data
            }
            ~ShmSend() = default;

            bool open();

        private:
            ShmSendErr write_data(ShmSendBlob send_blob);

    };
}