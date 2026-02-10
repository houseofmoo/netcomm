#if defined(EROIL_LINUX)

#include "events/semaphore.h"
#include <semaphore.h>
#include <cerrno>
#include <ctime>
#include <cstdint>

namespace eroil::evt {
    static sem_t* as_native(sem_handle p) noexcept {
        return static_cast<sem_t*>(p);
    }

    static sem_handle from_native(sem_t* h) noexcept {
        return static_cast<sem_handle>(h);
    }
    
    Semaphore::Semaphore() : Semaphore(0) {}

    Semaphore::Semaphore(uint32_t max_count) : m_sem(nullptr), m_max_count(max_count) {
        m_sem = from_native(new sem_t());
        sem_init(as_native(m_sem), 0, 0);
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
        
        other.m_sem = nullptr;
        other.m_max_count = 0;
    }

    Semaphore& Semaphore::operator=(Semaphore&& other) noexcept {
        if (this != &other) {
            close();

            m_sem = other.m_sem;
            m_max_count = other.m_max_count;
            
            other.m_sem = nullptr;
            other.m_max_count = 0;
        }
        return *this;
    }

    SemResult Semaphore::post() {
        if (m_sem == nullptr) return { SemErr::NotInitialized, SemOp::Post };

        if (sem_post(as_native(m_sem)) == -1) {
            int err = errno;
            switch (err) {
                case EINVAL: return { SemErr::NotInitialized, SemOp::Post };
                case EOVERFLOW: return { SemErr::MaxCountReached, SemOp::Post };
                default: return { SemErr::SysError, SemOp::Post }; // unknown error
            }
        }

        return { SemErr::None, SemOp::Post };
    }

    SemResult Semaphore::try_wait() {
        if (m_sem == nullptr) return { SemErr::NotInitialized, SemOp::TryWait };

        while (true) {
            if (sem_trywait(as_native(m_sem)) == 0) {
                return { SemErr::None, SemOp::TryWait };
            }

            const int err = errno;
            switch (err) {
                case EINTR: continue; // interrupted before acquiring
                case EAGAIN: return { SemErr::WouldBlock, SemOp::TryWait };
                case EINVAL: return { SemErr::NotInitialized, SemOp::TryWait };
                default: return { SemErr::SysError, SemOp::TryWait };
            }
        }
    }

    SemResult Semaphore::wait(uint32_t milliseconds) {
        if (m_sem == nullptr) return { SemErr::NotInitialized, SemOp::Wait };

        if (milliseconds == 0) {
            // infinite wait
            while (true) {
                if (sem_wait(as_native(m_sem)) == 0) {
                   return { SemErr::None, SemOp::Wait };
                }
                
                const int err = errno;
                switch (err) {
                    case EINTR: continue; // interrupted before acquiring
                    case EINVAL: return { SemErr::NotInitialized, SemOp::Wait };
                    default: return { SemErr::SysError, SemOp::Wait };
                }
            }
        } else {
            // timed wait
            timespec deadline{};
            if (!make_abs_deadline_realtime(deadline, milliseconds)) {
                return { SemErr::SysError, SemOp::Wait };
            }

            while (true) {
                if (sem_timedwait(as_native(m_sem), &deadline) == 0) {
                    return { SemErr::None, SemOp::Wait };
                }

                const int err = errno;
                switch (err) {
                    case EINTR: continue; // interrupted before acquiring
                    case ETIMEDOUT: return { SemErr::Timeout, SemOp::Wait };
                    case EINVAL: return { SemErr::NotInitialized, SemOp::Wait };
                    default: return { SemErr::SysError, SemOp::Wait };
                }
            }
        }
    }

    void Semaphore::close() {
        if (m_sem != nullptr) {
            sem_destroy(as_native(m_sem));
            //delete m_sem; // TODO: double check this is correct
            m_sem = nullptr;
        }
    }
}

#endif