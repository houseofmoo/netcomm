#include "events/named_event.h"
#include "windows_hdr.h"

namespace eroil::evt {
    static HANDLE as_native(sem_handle p) noexcept {
        return reinterpret_cast<HANDLE>(p);
    }

    static sem_handle as_sem_handle(HANDLE h) noexcept {
        return reinterpret_cast<sem_handle>(h);
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

    NamedEvent::NamedEvent(Label label, NodeId dest_node_id) :
        m_label_id(label), m_destination_id(dest_node_id), m_sem(nullptr) {}

    NamedEvent::~NamedEvent() {
        close();
    }

    std::string NamedEvent::name() const {
        return "Local\\eroil.evt." + std::to_string(m_destination_id) + "_" + std::to_string(m_label_id);
    }

    NamedEventErr NamedEvent::open() {
        if (m_sem != nullptr) return NamedEventErr::DoubleOpen;

        auto wname = to_windows_wstring(name());
        if (wname.empty()) {
            m_sem = nullptr;
            return NamedEventErr::InvalidName;
        }

        const BOOL manual_reset = FALSE; // auto-reset
        const BOOL initial_state = FALSE;

        HANDLE handle = ::CreateEventW(
            nullptr,
            manual_reset,
            initial_state,
            wname.c_str()
        );

        if (handle == nullptr) {
            m_sem = nullptr;
            return NamedEventErr::OpenFailed;
        }

        m_sem = as_sem_handle(handle);
        return NamedEventErr::None;
    }

    NamedEventErr NamedEvent::post() const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;
        if (!::SetEvent(as_native(m_sem))) return NamedEventErr::SignalFailed;
        return NamedEventErr::None;
    }

    NamedEventErr NamedEvent::try_wait() const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;
        DWORD rc = ::WaitForSingleObject(as_native(m_sem), 0);
        if (rc == WAIT_OBJECT_0) return NamedEventErr::None;
        if (rc == WAIT_TIMEOUT)  return NamedEventErr::Timeout;
        return NamedEventErr::WaitFailed;
    }

    NamedEventErr NamedEvent::wait() const {
        if (m_sem == nullptr) return NamedEventErr::NotInitialized;
        DWORD rc = ::WaitForSingleObject(as_native(m_sem), INFINITE);
        if (rc == WAIT_OBJECT_0) return NamedEventErr::None;
        return NamedEventErr::WaitFailed;
    }

    NamedEventErr NamedEvent::wait(uint32_t milliseconds) const {
            if (m_sem == nullptr) return NamedEventErr::NotInitialized;

            // prevent infinite wait...but this is a long ass time to wait
            DWORD timeout = (milliseconds == INFINITE) 
                ? static_cast<DWORD>(milliseconds - 1) 
                : static_cast<DWORD>(milliseconds);

            DWORD rc = ::WaitForSingleObject(as_native(m_sem), timeout);
            if (rc == WAIT_OBJECT_0) return NamedEventErr::None;
            if (rc == WAIT_TIMEOUT)  return NamedEventErr::Timeout;
            return NamedEventErr::WaitFailed;
    }

    void NamedEvent::close() {
        if (m_sem != nullptr) {
            ::CloseHandle(as_native(m_sem));
            m_sem = nullptr;
        }
    }   
}