#if defined(EROIL_WIN32)

#include "events/named_semaphore.h"
#include "windows_hdr.h"
#include <eROIL/print.h>

namespace eroil::evt {
    static HANDLE as_native(sem_handle p) noexcept {
        return static_cast<HANDLE>(p);
    }

    static sem_handle from_native(HANDLE h) noexcept {
        return static_cast<sem_handle>(h);
    }

    // static NamedSemResult do_wait(sem_handle handle, DWORD timeout_ms) {
    //     if (handle == nullptr) return NamedSemResult::Code::NotInitialized;
    //     DWORD rc = ::WaitForSingleObject(as_native(handle), timeout_ms);
    //     switch (rc) {
    //         case WAIT_OBJECT_0: return NamedSemResult::Code::None;
    //         case WAIT_TIMEOUT: return (timeout_ms == 0) ? NamedSemResult::Code::WouldBlock : NamedSemResult::Code::Timeout;
    //         case WAIT_FAILED: return NamedSemResult::Code::SysError;
    //         default: return NamedSemResult::Code::SysError;
    //     }
    // }

    static std::wstring to_windows_wstring(const std::string& s) {
        if (s.empty()) return {};

        int len = ::MultiByteToWideChar(
            CP_UTF8, 
            MB_ERR_INVALID_CHARS, 
            s.data(), 
            static_cast<int>(s.size()), 
            nullptr, 
            0
        );
        if (len <= 0) return {};

        std::wstring out(static_cast<size_t>(len), L'\0');
        int wrote = ::MultiByteToWideChar(
            CP_UTF8, 
            MB_ERR_INVALID_CHARS, 
            s.data(), 
            static_cast<int>(s.size()), 
            out.data(), 
            len
        );
        if (wrote != len) return {};
        return out;
    }

    NamedSemaphore::NamedSemaphore(NodeId id) : m_dst_id(id), m_sem(nullptr) {
        NamedSemResult result = open();
        if (!result.ok()) {
            ERR_PRINT("failed to open named event m_dst_id=", id);
        }
    }

    NamedSemaphore::~NamedSemaphore() {
        close();
    }

    NamedSemaphore::NamedSemaphore(NamedSemaphore&& other) noexcept : 
        m_dst_id(other.m_dst_id),
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
        return "Local\\eroil.evt." + std::to_string(m_dst_id);
    }

    NamedSemResult NamedSemaphore::open() {
        if (m_sem != nullptr) return { NamedSemErr::DoubleOpen, NamedSemOp::Open };

        std::wstring wname = to_windows_wstring(name());
        if (wname.empty()) {
            m_sem = nullptr;
            return { NamedSemErr::InvalidName, NamedSemOp::Open };
        }

        //const BOOL manual_reset = FALSE; // auto-reset
        //const BOOL initial_state = FALSE;

        // HANDLE handle = ::CreateEventW(
        //     nullptr,
        //     manual_reset,
        //     initial_state,
        //     wname.c_str()
        // );

        // counting semaphore linux style
        HANDLE handle = ::CreateSemaphoreW(
            nullptr,
            0,
            LONG_MAX,
            wname.c_str()
        );

        if (handle == nullptr) {
            m_sem = nullptr;
            return { NamedSemErr::OpenFailed, NamedSemOp::Open };
        }

        m_sem = from_native(handle);
        return { NamedSemErr::None, NamedSemOp::Open };
    }

    NamedSemResult NamedSemaphore::post() const {
        if (m_sem == nullptr) return { NamedSemErr::NotInitialized, NamedSemOp::Post };
        if (!::ReleaseSemaphore(as_native(m_sem), 1, nullptr)) {
            DWORD e = ::GetLastError();
            if (e == ERROR_TOO_MANY_POSTS) return { NamedSemErr::MaxCount, NamedSemOp::Post };
            return { NamedSemErr::SignalFailed, NamedSemOp::Post };
        }

        return { NamedSemErr::None, NamedSemOp::Post };
    }

    NamedSemResult NamedSemaphore::try_wait() const {
        if (m_sem == nullptr) return { NamedSemErr::NotInitialized, NamedSemOp::TryWait };
        DWORD rc = ::WaitForSingleObject(as_native(m_sem), 0);
        switch (rc) {
            case WAIT_OBJECT_0: return { NamedSemErr::None, NamedSemOp::TryWait };
            case WAIT_TIMEOUT: return { NamedSemErr::WouldBlock, NamedSemOp::TryWait };
            case WAIT_FAILED: return { NamedSemErr::SysError, NamedSemOp::TryWait };
            default: return { NamedSemErr::SysError, NamedSemOp::TryWait };;
        }
    }

    NamedSemResult NamedSemaphore::wait(uint32_t milliseconds) const {
        if (m_sem == nullptr) return { NamedSemErr::NotInitialized, NamedSemOp::Wait };
        
        DWORD timeout = static_cast<DWORD>(milliseconds);
        if (milliseconds == 0) {
            timeout = INFINITE;
        }

        DWORD rc = ::WaitForSingleObject(as_native(m_sem), timeout);
        switch (rc) {
            case WAIT_OBJECT_0: return { NamedSemErr::None, NamedSemOp::Wait };
            case WAIT_TIMEOUT: return { NamedSemErr::Timeout, NamedSemOp::Wait };
            case WAIT_FAILED: return { NamedSemErr::SysError, NamedSemOp::Wait };
            default: return { NamedSemErr::SysError, NamedSemOp::Wait };;
        }
    }

    void NamedSemaphore::close() {
        if (m_sem != nullptr) {
            ::CloseHandle(as_native(m_sem));
            m_sem = nullptr;
        }
    }   
}

#endif