#if defined(EROIL_WIN32)

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

    // static SemResult::Code do_wait(sem_handle handle, DWORD timeout_ms) {
    //     if (handle == nullptr) return SemResult::Code::NotInitialized;

    //     DWORD rc = ::WaitForSingleObject(as_native(handle), timeout_ms);
    //     switch (rc) {
    //         case WAIT_OBJECT_0: return SemResult::Code::None;
    //         case WAIT_TIMEOUT: return (timeout_ms == 0) ? SemResult::Code::WouldBlock : SemResult::Code::Timeout;
    //         case WAIT_FAILED: return SemResult::Code::SysError;
    //         default: return SemResult::Code::SysError;
    //     }
    // }

    Semaphore::Semaphore() : Semaphore(0) {}

    Semaphore::Semaphore(uint32_t max_count) : m_sem(nullptr), m_max_count(max_count) {
        if (m_max_count == 0) m_max_count = std::numeric_limits<long>::max();
        LONG max = static_cast<LONG>(m_max_count);
        m_sem = from_native(::CreateSemaphoreW(nullptr, 0, max, nullptr));
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

        if (!::ReleaseSemaphore(m_sem, 1, nullptr)) {
            DWORD err = ::GetLastError();
            if (err == ERROR_TOO_MANY_POSTS) return { SemErr::MaxCountReached, SemOp::Post };
            return { SemErr::SysError, SemOp::Post };
        }
        return { SemErr::None, SemOp::Post };
    }

    SemResult Semaphore::try_wait() {
        if (m_sem == nullptr) return { SemErr::NotInitialized, SemOp::TryWait };

        DWORD rc = ::WaitForSingleObject(as_native(m_sem), 0);
        switch (rc) {
            case WAIT_OBJECT_0: return { SemErr::None, SemOp::TryWait };
            case WAIT_TIMEOUT: return { SemErr::WouldBlock, SemOp::TryWait };
            case WAIT_FAILED: return { SemErr::SysError, SemOp::TryWait };
            default: return { SemErr::SysError, SemOp::TryWait };
        }
    }

    SemResult Semaphore::wait(uint32_t milliseconds) {
        if (m_sem == nullptr) return { SemErr::NotInitialized, SemOp::Wait };

        DWORD timeout = static_cast<DWORD>(milliseconds);
        if (milliseconds <= 0) {
            timeout = INFINITE;
        }

        DWORD rc = ::WaitForSingleObject(as_native(m_sem), timeout);
        switch (rc) {
            case WAIT_OBJECT_0: return { SemErr::None, SemOp::Wait };
            case WAIT_TIMEOUT: return { SemErr::Timeout, SemOp::Wait };
            case WAIT_FAILED: return { SemErr::SysError, SemOp::Wait };
            default: return { SemErr::SysError, SemOp::Wait };
        }
    }

    void Semaphore::close() {
        if (m_sem != nullptr) {
            ::CloseHandle(as_native(m_sem));
            m_sem = nullptr;
        }
    }
}

#endif