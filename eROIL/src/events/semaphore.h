#pragma once
#include <string_view>
#include "types/const_types.h"
#include "macros.h"

namespace eroil::evt {
    enum class SemErr {
        None,
        NotInitialized,
        Timeout,
        WouldBlock,
        MaxCountReached,
        SysError,
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

            SemErr post();
            SemErr try_wait();
            SemErr wait(uint32_t milliseconds = 0);
            void close();
    };
}