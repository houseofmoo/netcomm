#pragma once
#include "types/types.h"

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

            // do not copy
            Semaphore(const Semaphore&) = delete;
            Semaphore& operator=(const Semaphore&) = delete;

            // allow move
            Semaphore(Semaphore&& other) noexcept;
            Semaphore& operator=(Semaphore&& other) noexcept;

            SemOpErr post();
            SemOpErr try_wait();
            SemOpErr wait(uint32_t milliseconds = 0);
            void close();
    };
}