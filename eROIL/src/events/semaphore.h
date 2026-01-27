#pragma once
#include "types/const_types.h"
#include "types/macros.h"

namespace eroil::evt {
    enum class SemOpErr {
        None,
        NotInitialized,
        Timeout,
        WouldBlock,
        MaxCountReached,
        SysError,
    };

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

            SemOpErr post();
            SemOpErr try_wait();
            SemOpErr wait(uint32_t milliseconds = 0);
            void close();
    };
}