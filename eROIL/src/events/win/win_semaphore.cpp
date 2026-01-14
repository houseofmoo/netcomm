#ifdef EROIL_WIN32

#include "events/semaphore.h"
#include "windows_hdr.h"
#include <limits>

namespace eroil::evt {
    static HANDLE as_native(sem_handle p) noexcept {
        return static_cast<HANDLE>(p);
    }

    static sem_handle from_native(HANDLE h) noexcept {
        return static_cast<sem_handle>(h);
    }

    static SemOpErr do_wait(sem_handle handle, DWORD timeout_ms) {
        if (handle == nullptr) return SemOpErr::NotInitialized;

        DWORD rc = ::WaitForSingleObject(as_native(handle), timeout_ms);

        switch (rc) {
            case WAIT_OBJECT_0: return SemOpErr::None;
            case WAIT_TIMEOUT: return (timeout_ms == 0) ? SemOpErr::WouldBlock : SemOpErr::Timeout;
            case WAIT_FAILED: return SemOpErr::SysError;
            default: return SemOpErr::SysError;
        }
    }

    Semaphore::Semaphore() : Semaphore(0) {}

    Semaphore::Semaphore(uint32_t max_count) : m_sem(nullptr), m_max_count(max_count) {
        if (m_max_count == 0) m_max_count = std::numeric_limits<long>::max();
        m_sem = from_native(::CreateSemaphoreW(nullptr, 0, m_max_count, nullptr));
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

    SemOpErr Semaphore::post() {
        if (m_sem == nullptr) return SemOpErr::NotInitialized;

        if (!::ReleaseSemaphore(m_sem, 1, nullptr)) {
            DWORD err = ::GetLastError();
            if (err == ERROR_TOO_MANY_POSTS) return SemOpErr::MaxCountReached;
            return SemOpErr::SysError;
        }
        return SemOpErr::None;
    }

    SemOpErr Semaphore::try_wait() {
        return do_wait(m_sem, 0);
    }

    SemOpErr Semaphore::wait(uint32_t milliseconds) {
        DWORD timeout = static_cast<DWORD>(milliseconds);
        if (milliseconds <= 0) {
            timeout = INFINITE;
        }
        return do_wait(m_sem, timeout);
    }

    void Semaphore::close() {
        if (m_sem != nullptr) {
            ::CloseHandle(as_native(m_sem));
            m_sem = nullptr;
        }
    }
}

#endif