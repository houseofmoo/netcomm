#pragma once
#include "shm.h"
#include "events/named_event.h"
#include "types/const_types.h"
#include "shm_header.h"
#include "types/macros.h"

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

    class ShmRecv {
        private:
            NodeId m_id;
            Shm m_shm;
            evt::NamedEvent m_event;

        public:
            ShmRecv(NodeId id);
            ~ShmRecv();

            EROIL_NO_COPY(ShmRecv)
            EROIL_NO_MOVE(ShmRecv)

            bool create_or_open();
            void close();
            void init_as_new();
            void reinit();
            evt::NamedEventErr wait();
            ShmRecvErr read_data(ShmRecvPayload& recv);
            void flush_backlog();
    };
}