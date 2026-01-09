#include "events/semaphore.h"
#include "windows_hdr.h"

namespace eroil::evt {
    static SemOpErr do_wait(sem_handle handle, DWORD timeout_ms) {
        if (handle == nullptr) return SemOpErr::InvalidSem;

        DWORD rc = ::WaitForSingleObject(handle, timeout_ms);

        switch (rc) {
            case WAIT_OBJECT_0: return SemOpErr::None;
            case WAIT_TIMEOUT: return (timeout_ms == 0) ? SemOpErr::WouldBlock : SemOpErr::Timeout;
            case WAIT_FAILED: return SemOpErr::SysError;
            default: return SemOpErr::SysError;
        }
    }

    Semaphore::Semaphore() : Semaphore(1000) {}

    Semaphore::Semaphore(uint32_t max_count) : m_sem(nullptr), m_max_count(max_count) {
        m_sem = ::CreateSemaphoreW(nullptr, 0, max_count, nullptr);
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

    SemOpErr Semaphore::wait() {
        return do_wait(m_sem, INFINITE);

    }

    SemOpErr Semaphore::timed_wait(uint32_t milliseconds) {
        return do_wait(m_sem, static_cast<DWORD>(milliseconds));
    }

    SemOpErr Semaphore::try_wait() {
        return do_wait(m_sem, 0);
    }

    SemOpErr Semaphore::post() {
        if (m_sem == nullptr) return SemOpErr::InvalidSem;

        if (!::ReleaseSemaphore(m_sem, 1, nullptr)) {
            DWORD err = ::GetLastError();
            if (err == ERROR_TOO_MANY_POSTS) return SemOpErr::MaxCountReached;
            return SemOpErr::SysError;
        }
        return SemOpErr::None;
    }

    void Semaphore::close() {
        if (m_sem != nullptr) {
            ::CloseHandle(m_sem);
            m_sem = nullptr;
        }
    }
}