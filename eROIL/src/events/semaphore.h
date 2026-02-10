#pragma once
#include "types/const_types.h"
#include "macros.h"

namespace eroil::evt {
    enum class SemErr {
        None = 0,
        NotInitialized,
        Timeout,
        WouldBlock,
        MaxCountReached,
        SysError,
    };

    enum class SemOp {
        Post,
        TryWait,
        Wait
    };
    
    struct SemResult {
        SemErr code;
        SemOp op;

        bool ok() const noexcept {
            return code == SemErr::None;
        }

        std::string_view code_to_string() const noexcept {
            switch (code) {
                case SemErr::None: return "None";
                case SemErr::NotInitialized: return "NotInitialized";
                case SemErr::Timeout: return "Timeout";
                case SemErr::WouldBlock: return "WouldBlock";
                case SemErr::MaxCountReached: return "MaxCountReached";
                case SemErr::SysError: return "SysError";
                default: return "Unknown - error is undefined";
            }
        }

        std::string_view op_to_string() const noexcept {
            switch (op) {
                case SemOp::Post: return "Post";
                case SemOp::TryWait: return "TryWait";
                case SemOp::Wait: return "Wait";
                default: return "Unknown - op is undefined";
            }
        }
    };

    // counting semaphore
    class Semaphore {
        private:
            sem_handle m_sem;
            uint32_t m_max_count = 0;

        public:
            Semaphore();
            explicit Semaphore(uint32_t max_count);
            ~Semaphore();

            EROIL_NO_COPY(Semaphore)
            EROIL_DECL_MOVE(Semaphore)

            NO_DISCARD SemResult post();
            NO_DISCARD SemResult try_wait();
            NO_DISCARD SemResult wait(uint32_t milliseconds = 0);
            void close();
    };
}