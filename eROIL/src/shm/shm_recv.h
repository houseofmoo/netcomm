#pragma once
#include <memory>
#include "shm.h"
#include "events/named_semaphore.h"
#include "types/const_types.h"
#include "shm_header.h"
#include "macros.h"

namespace eroil::shm {

    enum class ShmRecvErr {
        None = 0,           // success
        BlockNotInitialized,// hard error, we should only be running when the block is initialized
        NoRecords,          // wait for event
        NotYetPublished,    // try again later
        TailCorruption,     // re-init
        BlockCorrupted,     // re-init
        UnknownError        // re-init
    };

    enum class ShmRecvOp {
        Recv
    };

    struct ShmRecvResult {
        ShmRecvErr code;
        ShmRecvOp op;

        bool ok() const noexcept {
            return code == ShmRecvErr::None;
        }

        std::string_view code_to_string() const noexcept {
            switch (code) {
                case ShmRecvErr::None: return "None";
                case ShmRecvErr::BlockNotInitialized: return "BlockNotInitialized";
                case ShmRecvErr::NoRecords: return "NoRecords";
                case ShmRecvErr::NotYetPublished: return "NotYetPublished";
                case ShmRecvErr::TailCorruption: return "TailCorruption";
                case ShmRecvErr::BlockCorrupted: return "BlockCorrupted";
                case ShmRecvErr::UnknownError: return "UnknownError";
                default: return "Unknown - error is undefined";
            }
        }

        std::string_view op_to_string() const noexcept {
            switch (op) {
                case ShmRecvOp::Recv: return "Recv";
                default: return "Unknown - op is undefined";
            }
        }
    };

    struct ShmRecvData {
        ShmRecvResult result = { ShmRecvErr::None, ShmRecvOp::Recv };
        NodeId source_id = INVALID_NODE;
        Label label = INVALID_LABEL;
        uint32_t user_seq = 0;
        size_t buf_size = 0;
        std::unique_ptr<std::byte[]> buf = nullptr;

        explicit ShmRecvData() = default;
        explicit ShmRecvData(ShmRecvErr r) {
            result.code = r;
            result.op = ShmRecvOp::Recv;
        }
        explicit ShmRecvData(ShmRecvErr r, ShmRecvOp o) {
            result.code = r;
            result.op = o;
        }
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
            NO_DISCARD evt::NamedSemResult wait();
            NO_DISCARD ShmRecvData recv();
            void flush_backlog();
    };
}