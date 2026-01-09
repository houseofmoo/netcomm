#pragma once
#include "types/types.h"

namespace eroil::evt {
    enum class SemOpErr {
        None,
        InvalidSem,
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

            Semaphore(const Semaphore&) = delete;
            Semaphore& operator=(const Semaphore&) = delete;

            Semaphore(Semaphore&& other) noexcept;
            Semaphore& operator=(Semaphore&& other) noexcept;

            SemOpErr wait();
            SemOpErr timed_wait(uint32_t milliseconds);
            SemOpErr try_wait();
            SemOpErr post();
            void close();
    };
}