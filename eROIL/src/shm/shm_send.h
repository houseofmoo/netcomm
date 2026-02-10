#pragma once
#include "shm.h"
#include "events/named_semaphore.h"
#include "types/const_types.h"
#include "shm_header.h"
#include "macros.h"

namespace eroil::shm {
    enum class ShmSendErr {
        None = 0,           // success
        InvalidOffset,      // offset into header or meta invalid
        BlockNotInitialized,// try again or drop
        BlockReinitialized, // try again or drop
        NotEnoughSpace,     // try again or drop
        SizeTooLarge,       // hard error
        CouldNotAllocate,   // hard error
        AllocatorCorrupted  // fatal, maybe retry a few times
    };

    enum class ShmSendOp {
        Send
    };

    struct ShmSendResult {
        ShmSendErr code;
        ShmSendOp op;

        bool ok() const noexcept {
            return code == ShmSendErr::None;
        }

        std::string_view code_to_string() const noexcept {
            switch (code) {
                case ShmSendErr::None: return "None";
                case ShmSendErr::InvalidOffset: return "InvalidOffset";
                case ShmSendErr::BlockNotInitialized: return "BlockNotInitialized";
                case ShmSendErr::BlockReinitialized: return "BlockReinitialized";
                case ShmSendErr::NotEnoughSpace: return "NotEnoughSpace";
                case ShmSendErr::SizeTooLarge: return "SizeTooLarge";
                case ShmSendErr::CouldNotAllocate: return "CouldNotAllocate";
                case ShmSendErr::AllocatorCorrupted: return "AllocatorCorrupted";
                default: return "Unknown - error is undefined";
            }
        }

        std::string_view op_to_string() const noexcept {
            switch (op) {
                case ShmSendOp::Send: return "Send";
                default: return "Unknown - op is undefined";
            }
        }
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
            NO_DISCARD ShmSendResult send(const NodeId id, 
                                          const Label label, 
                                          const uint32_t seq, 
                                          const size_t buf_size, 
                                          const std::byte* buf);
    };
}