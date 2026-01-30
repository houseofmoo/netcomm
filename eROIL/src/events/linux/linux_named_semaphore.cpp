#if defined(EROIL_LINUX)

#include "events/named_semaphore.h"
#include <eROIL/print.h>

#include <semaphore.h>
#include <fcntl.h>      // O_CREAT
#include <sys/stat.h>   // mode constants
#include <cerrno>
#include <cstring>
#include <ctime>
#include <string>

namespace eroil::evt {
    static sem_t* as_native(sem_handle p) noexcept {
        return static_cast<sem_t*>(p);
    }

    static sem_handle from_native(sem_t* h) noexcept {
        return static_cast<sem_handle>(h);
    }

    static NamedSemErr map_wait_errno(int e) noexcept {
        switch (e) {
            case EINTR: return NamedSemErr::SysError; // normally retry on this... if we get here somethigns fooked
            case ETIMEDOUT: return NamedSemErr::Timeout;
            case EAGAIN: return NamedSemErr::SysError;
            case EINVAL: return NamedSemErr::NotInitialized;
            default: return NamedSemErr::SysError;
        }
    }

    static bool make_abs_deadline_realtime(timespec& out_abs, uint32_t timeout_ms) noexcept {
        if (::clock_gettime(CLOCK_REALTIME, &out_abs) != 0) {
            return false;
        }

        const long add_ns = static_cast<long>((timeout_ms % 1000u) * 1'000'000u);
        out_abs.tv_sec += static_cast<time_t>(timeout_ms / 1000u);
        out_abs.tv_nsec += add_ns;

        if (out_abs.tv_nsec >= 1'000'000'000L) {
            out_abs.tv_sec += 1;
            out_abs.tv_nsec -= 1'000'000'000L;
        }
        return true;
    }

    NamedSemaphore::NamedSemaphore(NodeId id) : m_dst_id(id), m_sem(nullptr) {
        if (open() != NamedSemErr::None) {
            ERR_PRINT("failed to open named event m_dst_id=", id);
        }
    }

    NamedSemaphore::~NamedSemaphore() {
        close();
    }

    NamedSemaphore::NamedSemaphore(NamedSemaphore&& other) noexcept
        : m_dst_id(other.m_dst_id),
          m_sem(other.m_sem) {
        other.m_dst_id = INVALID_NODE;
        other.m_sem = nullptr;
    }

    NamedSemaphore& NamedSemaphore::operator=(NamedSemaphore&& other) noexcept {
        if (this != &other) {
            close();
            m_dst_id = other.m_dst_id;
            m_sem = other.m_sem;

            other.m_dst_id = INVALID_NODE;
            other.m_sem = nullptr;
        }
        return *this;
    }

    std::string NamedSemaphore::name() const {
        return "/eroil.evt." + std::to_string(m_dst_id);
    }

    NamedSemErr NamedSemaphore::open() {
        if (m_sem != nullptr) return NamedSemErr::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            m_sem = nullptr;
            return NamedSemErr::InvalidName;
        }

        constexpr mode_t perms = 0777;
        auto sem = sem_open(n.c_str(), O_CREAT, perms, 0);
        if (sem == SEM_FAILED) {
            sem = nullptr;
            return NamedSemErr::OpenFailed;
        }

        m_sem = from_native(sem);
        return NamedSemErr::None;
    }

    NamedSemErr NamedSemaphore::post() const {
        if (m_sem == nullptr) return NamedSemErr::NotInitialized;

        if (sem_post(as_native(m_sem)) != 0) {
            if (errno == EOVERFLOW) return NamedSemErr::MaxCount;
            return NamedSemErr::SignalFailed;
        }

        return NamedSemErr::None;
    }

    NamedSemErr NamedSemaphore::try_wait() const {
        if (m_sem == nullptr) return NamedSemErr::NotInitialized;

        for (;;) {
            if (sem_trywait(as_native(m_sem)) == 0) {
                return NamedSemErr::None;
            }
            const int e = errno;
            if (e == EINTR) continue;
            if (e == EAGAIN) return NamedSemErr::WouldBlock;

            return map_wait_errno(e);
        }
    }

    NamedSemErr NamedSemaphore::wait(uint32_t milliseconds) const {
        if (m_sem == nullptr) return NamedSemErr::NotInitialized;

        if (milliseconds == 0) {
            for (;;) {
                if (sem_wait(as_native(m_sem)) == 0) {
                    return NamedSemErr::None;
                }
                const int e = errno;
                if (e == EINTR) continue;
                return map_wait_errno(e);
            }
        }

        timespec deadline{};
        if (!make_abs_deadline_realtime(deadline, milliseconds)) {
            return NamedSemErr::SysError;
        }

        for (;;) {
            if (sem_timedwait(as_native(m_sem), &deadline) == 0) {
                return NamedSemErr::None;
            }
            const int e = errno;
            if (e == EINTR) continue;
            return map_wait_errno(e);
        }
    }

    void NamedSemaphore::close() {
        if (m_sem != nullptr) {
            sem_close(as_native(m_sem));
            //sem_unlink(name().c_str()); // deletes sem file
            m_sem = nullptr;
        }
    }
}

#endif