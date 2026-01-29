#if defined(EROIL_WIN32)

#include "events/named_event.h"
#include "windows_hdr.h"
#include <eROIL/print.h>

namespace eroil::evt {
    static HANDLE as_native(sem_handle p) noexcept {
        return static_cast<HANDLE>(p);
    }

    static sem_handle from_native(HANDLE h) noexcept {
        return static_cast<sem_handle>(h);
    }

    static NamedEventErr do_wait(sem_handle handle, DWORD timeout_ms) {
        if (handle == nullptr) return NamedEventErr::NotInitialized;
        DWORD rc = ::WaitForSingleObject(as_native(handle), timeout_ms);
        switch (rc) {
            case WAIT_OBJECT_0: return NamedEventErr::None;
            case WAIT_TIMEOUT: return (timeout_ms == 0) ? NamedEventErr::WouldBlock : NamedEventErr::Timeout;
            case WAIT_FAILED: return NamedEventErr::SysError;
            default: return NamedEventErr::SysError;
        }
    }

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

        std::wstring out((size_t)len, L'\0');
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

    NamedEvent::NamedEvent(NodeId id) : m_dst_id(id), m_sem(nullptr) {
        if (open() != NamedEventErr::None) {
            ERR_PRINT("failed to open named event m_dst_id=", id);
        }
    }

    NamedEvent::~NamedEvent() {
        close();
    }

    NamedEvent::NamedEvent(NamedEvent&& other) noexcept : 
        m_dst_id(other.m_dst_id),
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
        return "Local\\eroil.evt." + std::to_string(m_dst_id);
    }

    NamedEventErr NamedEvent::open() {
        if (m_sem != nullptr) return NamedEventErr::DoubleOpen;

        std::wstring wname = to_windows_wstring(name());
        if (wname.empty()) {
            m_sem = nullptr;
            return NamedEventErr::InvalidName;
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
            return NamedEventErr::OpenFailed;
        }

        m_sem = from_native(handle);
        return NamedEventErr::None;
    }

    NamedEventErr NamedEvent::post() const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;
        if (!::ReleaseSemaphore(as_native(m_sem), 1, nullptr)) {
            DWORD e = ::GetLastError();
            if (e == ERROR_TOO_MANY_POSTS) return NamedEventErr::MaxCount;
            return NamedEventErr::SignalFailed;
        }

        // if (!::SetEvent(as_native(m_sem))) {
        //     return NamedEventErr::SignalFailed;
        // }
        return NamedEventErr::None;
    }

    NamedEventErr NamedEvent::try_wait() const {
        return do_wait(m_sem, 0);
    }

    NamedEventErr NamedEvent::wait(uint32_t milliseconds) const {
        DWORD timeout = static_cast<DWORD>(milliseconds);
        if (milliseconds == 0) {
            timeout = INFINITE;
        }
        return do_wait(m_sem, timeout);
    }

    void NamedEvent::close() {
        if (m_sem != nullptr) {
            ::CloseHandle(as_native(m_sem));
            m_sem = nullptr;
        }
    }   
}

#endif