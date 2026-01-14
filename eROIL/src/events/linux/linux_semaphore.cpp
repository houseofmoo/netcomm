#ifdef EROIL_LINUX

#include "events/semaphore.h"
#include <semaphore.h>
#include <cerrno>
#include <ctime>
#include <cstdint>

namespace eroil::evt {
    Semaphore::Semaphore() : Semaphore(0) {}

    Semaphore::Semaphore(uint32_t max_count) : m_sem{}, m_max_count(max_count), m_valid(true) {
        sem_init(&m_sem, 0, 0);
    }

    static bool make_abs_deadline_realtime(timespec& deadline, uint32_t timeout_ms) noexcept {
        if (::clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            return false;
        }

        const long add_ns = static_cast<long>((timeout_ms % 1000u) * 1'000'000u);
        deadline.tv_sec += static_cast<time_t>(timeout_ms / 1000u);
        deadline.tv_nsec += add_ns;

        if (deadline.tv_nsec >= 1'000'000'000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1'000'000'000L;
        }
        return true;
    }

    Semaphore::~Semaphore() {
        close();
    }

    Semaphore::Semaphore(Semaphore&& other) noexcept {
        m_sem = other.m_sem;
        m_max_count = other.m_max_count;
        m_valid = true;
        
        //other.m_sem = nullptr;
        other.m_max_count = 0;
        other.m_valid = false;
    }

    Semaphore& Semaphore::operator=(Semaphore&& other) noexcept {
        if (this != &other) {
            close();

            m_sem = other.m_sem;
            m_max_count = other.m_max_count;
            m_valid = true;
            
            //other.m_sem = nullptr;
            other.m_max_count = 0;
            other.m_valid = false;
        }
        return *this;
    }

    SemOpErr Semaphore::post() {
        if (!m_valid) return SemOpErr::NotInitialized;

        if (sem_post(&m_sem) == -1) {
            int err = errno;
            switch (err) {
                case EINVAL: return SemOpErr::NotInitialized;
                case EOVERFLOW: return SemOpErr::MaxCountReached;
                default: return SemOpErr::SysError; // unknown error
            }
        }

        return SemOpErr::None;
    }

    SemOpErr Semaphore::try_wait() {
        if (!m_valid) return SemOpErr::NotInitialized;

        while (true) {
            if (sem_trywait(&m_sem) == 0) {
                return SemOpErr::None;
            }

            const int err = errno;
            switch (err) {
                case EINTR: continue; // interrupted before acquiring
                case EAGAIN: return SemOpErr::WouldBlock;
                case EINVAL: return SemOpErr::NotInitialized;
                default: return SemOpErr::SysError;
            }
        }
    }

    SemOpErr Semaphore::wait(uint32_t milliseconds) {
        if (!m_valid) return SemOpErr::NotInitialized;

        if (milliseconds == 0) {
            // infinite wait
            while (true) {
                if (sem_wait(&m_sem) == 0) {
                   return SemOpErr::None;
                }
                
                const int err = errno;
                switch (err) {
                    case EINTR: continue; // interrupted before acquiring
                    case EINVAL: return SemOpErr::NotInitialized;
                    default: return SemOpErr::SysError;
                }
            }
        } else {
            // timed wait
            timespec deadline{};
            if (!make_abs_deadline_realtime(deadline, milliseconds)) {
                return SemOpErr::SysError;
            }

            while (true) {
                if (sem_timedwait(&m_sem, &deadline) == 0) {
                    return SemOpErr::None;
                }

                const int err = errno;
                switch (err) {
                    case EINTR: continue; // interrupted before acquiring
                    case ETIMEDOUT: return SemOpErr::Timeout;
                    case EINVAL: return SemOpErr::NotInitialized;
                    default: return SemOpErr::SysError;
                }
            }

        }
    }

    void Semaphore::close() {
        if (m_valid) {
            sem_destroy(&m_sem);
            m_valid = false;
        }
    }
}

#endif