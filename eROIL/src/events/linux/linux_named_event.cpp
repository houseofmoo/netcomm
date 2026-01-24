#if defined(EROIL_LINUX)

#include "events/named_event.h"
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

    static NamedEventErr map_wait_errno(int e) noexcept {
        switch (e) {
            case EINTR: return NamedEventErr::SysError; // normally retry on this... if we get here somethigns fooked
            case ETIMEDOUT: return NamedEventErr::Timeout;
            case EAGAIN: return NamedEventErr::SysError;
            case EINVAL: return NamedEventErr::NotInitialized;
            default: return NamedEventErr::SysError;
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

    NamedEvent::NamedEvent(NodeId id) : m_dst_id(id), m_sem(nullptr) {
        if (open() != NamedEventErr::None) {
            ERR_PRINT("failed to open named event m_dst_id=", id);
        }
    }

    NamedEvent::~NamedEvent() {
        close();
    }

    NamedEvent::NamedEvent(NamedEvent&& other) noexcept
        : m_dst_id(other.m_dst_id),
          m_sem(other.m_sem) {
        other.m_dst_id = INVALID_NODE;
        other.m_sem = nullptr;
    }

    NamedEvent& NamedEvent::operator=(NamedEvent&& other) noexcept {
        if (this != &other) {
            close();
            m_dst_id = other.m_dst_id;
            m_sem = other.m_sem;

            other.m_dst_id = INVALID_NODE;
            other.m_sem = nullptr;
        }
        return *this;
    }

    std::string NamedEvent::name() const {
        return "/eroil.evt." + std::to_string(m_dst_id);
    }

    NamedEventErr NamedEvent::open() {
        if (m_sem != nullptr) return NamedEventErr::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            m_sem = nullptr;
            return NamedEventErr::InvalidName;
        }

        constexpr mode_t perms = 0777;
        auto sem = sem_open(n.c_str(), O_CREAT, perms, 0);
        if (sem == SEM_FAILED) {
            sem = nullptr;
            return NamedEventErr::OpenFailed;
        }

        m_sem = from_native(sem);
        return NamedEventErr::None;
    }

    NamedEventErr NamedEvent::post() const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;

        if (sem_post(as_native(m_sem)) != 0) {
            if (errno == EOVERFLOW) return NamedEventErr::MaxCount;
            return NamedEventErr::SignalFailed;
        }

        return NamedEventErr::None;
    }

    NamedEventErr NamedEvent::try_wait() const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;

        for (;;) {
            if (sem_trywait(as_native(m_sem)) == 0) {
                return NamedEventErr::None;
            }
            const int e = errno;
            if (e == EINTR) continue;
            if (e == EAGAIN) return NamedEventErr::WouldBlock;

            return map_wait_errno(e);
        }
    }

    NamedEventErr NamedEvent::wait(uint32_t milliseconds) const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;

        if (milliseconds <= 0) {
            for (;;) {
                if (sem_wait(as_native(m_sem)) == 0) {
                    return NamedEventErr::None;
                }
                const int e = errno;
                if (e == EINTR) continue;
                return map_wait_errno(e);
            }
        }

        timespec deadline{};
        if (!make_abs_deadline_realtime(deadline, milliseconds)) {
            return NamedEventErr::SysError;
        }

        for (;;) {
            if (sem_timedwait(as_native(m_sem), &deadline) == 0) {
                return NamedEventErr::None;
            }
            const int e = errno;
            if (e == EINTR) continue;
            return map_wait_errno(e);
        }
    }

    void NamedEvent::close() {
        if (m_sem != nullptr) {
            sem_close(as_native(m_sem));
            //sem_unlink(name().c_str()); // deletes sem file
            m_sem = nullptr;
        }
    }
}

#endif
