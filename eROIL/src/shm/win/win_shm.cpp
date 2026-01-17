#if defined(EROIL_WIN32)

#include "shm/shm.h"
#include "windows_hdr.h"
#include "types/types.h"
#include "shm/shm_header.h"

namespace eroil::shm {
    // static HANDLE as_native(shm_handle h) noexcept {
    //     return static_cast<HANDLE>(h);
    // }

    // static shm_handle from_native(HANDLE fd) noexcept {
    //     return static_cast<shm_handle>(fd);
    // }

    static std::wstring to_windows_wstring(const std::string& s) {
        if (s.empty()) return {};

        int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (len <= 0) return {};

        std::wstring out((size_t)len, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
        return out;
    }

    Shm::Shm(const Label label, const size_t label_size) : 
        m_label(label), m_label_size(label_size), m_handle(nullptr), m_view(nullptr) {}

    Shm::Shm(Shm&& other) noexcept : 
        m_label(other.m_label),
        m_label_size(other.m_label_size),
        m_handle(other.m_handle),
        m_view(other.m_view) {

        other.m_handle = nullptr;
        other.m_view   = nullptr;
        other.m_label  = 0;
        other.m_label_size = 0;
    }

    Shm& Shm::operator=(Shm&& other) noexcept {
        if (this != &other) {
            close();

            m_label = other.m_label;
            m_label_size = other.m_label_size;
            m_handle = other.m_handle;
            m_view = other.m_view;

            other.m_handle = nullptr;
            other.m_view = nullptr;
            other.m_label = 0;
            other.m_label_size = 0;
        }
        return *this;
    }

    bool Shm::is_valid() const noexcept {
        return m_handle != nullptr && m_view != nullptr;
    }

    std::string Shm::name() const noexcept {
        // cross session, but need admin rights
        //return "Global\\eroil.label." + std::to_string(m_label);

        // local session only
        return "Local\\eroil.label." + std::to_string(m_label);
    }

    ShmErr Shm::create() {
        if (is_valid()) return ShmErr::DoubleOpen;

        auto wname = to_windows_wstring(name());
        if (wname.empty()) {
            return ShmErr::InvalidName;
        }

        m_handle = ::CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(total_size()),
            wname.c_str()
        );

        if (m_handle == nullptr) {
            close();
            return ShmErr::UnknownError;
        }

        if (::GetLastError() == ERROR_ALREADY_EXISTS) {
            close();
            return ShmErr::AlreadyExists;;
        }

        m_view = ::MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (m_view == nullptr) {
            close();
            return ShmErr::FileMapFailed;
        }

        write_shm_header();
 
        return ShmErr::None;
    }

    ShmErr Shm::open() {
        if (is_valid()) return ShmErr::DoubleOpen;

        auto wname = to_windows_wstring(name());
        if (wname.empty()) {
            return ShmErr::InvalidName;
        }

         m_handle = ::OpenFileMappingW(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            wname.c_str()
        );

        if (m_handle == nullptr) {
            DWORD e = ::GetLastError();
            if (e == ERROR_FILE_NOT_FOUND) return ShmErr::DoesNotExist;
            return ShmErr::UnknownError;
        }

        m_view = ::MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (m_view == nullptr) {
            close();
            m_handle = nullptr;
            return ShmErr::FileMapFailed;
        }
   
        return validate_shm_header();
    }

    void Shm::close() noexcept {
        if (m_view != nullptr) {
            ::UnmapViewOfFile(m_view);
            m_view = nullptr;
        }

        if (m_handle != nullptr) {
            ::CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }
}

#endif