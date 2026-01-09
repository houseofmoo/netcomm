#include "shm/shm.h"
#include "windows_hdr.h"
#include <cstring>
#include <thread>
#include <chrono>

namespace eroil::shm {
    static std::wstring to_windows_wstring(const std::string& s) {
        if (s.empty()) return {};

        int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (len <= 0) return {};

        std::wstring out((size_t)len, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
        return out;
    }

    bool shm_exists(std::string name) {
        if (name.empty()) return false;

        auto wname = to_windows_wstring(name);
        HANDLE handle = ::CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            1,
            wname.c_str()
        );

        if (!handle)  {
            DWORD e = GetLastError();
            return e == ERROR_ACCESS_DENIED; 
        }

        bool exists = (GetLastError() == ERROR_ALREADY_EXISTS);
        ::CloseHandle(handle);
        return exists;
    }

    std::byte* Shm::data_ptr() const noexcept {
        if (m_view == nullptr) return nullptr;
        return static_cast<std::byte*>(m_view) + sizeof(ShmHeader);
    }

    Shm::Shm(const Label label, const size_t label_size) : 
        m_label(label), m_label_size(label_size), m_handle(nullptr), m_view(nullptr), m_valid(false) {}

    Shm::~Shm() {
        close();
    }

    std::string Shm::name() const noexcept {
        return "Local\\eroil.label." + std::to_string(m_label);
    }

    size_t Shm::size_bytes_total() const noexcept {
        return m_label_size + sizeof(ShmHeader);
    }

    ShmErr Shm::open_new() {
        if (m_valid) return ShmErr::DoubleOpen;

        auto wname = to_windows_wstring(name());
        if (wname.empty()) {
            return ShmErr::InvalidName;
        }

        m_handle = ::CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(size_bytes_total()),
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

        // write header to block
        auto hdr = ShmHeader{
            kShmMagic,
            kShmVersion,
            static_cast<uint16_t>(sizeof(ShmHeader)),
            static_cast<uint32_t>(m_label_size)
        };
        *static_cast<ShmHeader*>(m_view) = hdr;
 
        m_valid = true;
        return ShmErr::None;
    }

    ShmErr Shm::open_new_rety(const uint32_t attempts, const uint32_t wait_ms) {
        ShmErr error = ShmErr::None;
        for (uint32_t i = 0; i < attempts; ++i) {
            error = open_new();
            if (error == ShmErr::None) return error;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        }
        return error;
    }

    ShmErr Shm::open_existing() {
        if (m_valid) return ShmErr::DoubleOpen;

        auto wname = to_windows_wstring(name());
        if (wname.empty()) {
            return ShmErr::InvalidName;
        }

         m_handle = OpenFileMappingW(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            wname.c_str()
        );

        if (m_handle == nullptr) {
            DWORD e = GetLastError();
            m_valid = false;
            if (e == ERROR_FILE_NOT_FOUND) return ShmErr::DoesNotExist;
            return ShmErr::UnknownError;
        }

        m_view = MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (m_view == nullptr) {
            CloseHandle(m_handle);
            m_valid = false;
            return ShmErr::FileMapFailed;
        }

        // read header and validate
        const auto* hdr = static_cast<const ShmHeader*>(m_view);
        if (hdr->magic != kShmMagic ||
            hdr->version != kShmVersion ||
            hdr->header_size != sizeof(ShmHeader) ||
            hdr->label_size != static_cast<uint32_t>(m_label_size)) {
            close();
            return ShmErr::LayoutMismatch;
        }

        m_valid = true;
        return ShmErr::None;
    }

    ShmErr Shm::open_existing_rety(const uint32_t attempts, const uint32_t wait_ms) {
        ShmErr error = ShmErr::None;
        for (uint32_t i = 0; i < attempts; ++i) {
            error = open_existing();
            if (error == ShmErr::None) return error;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        }
        return error;
    }

    ShmOpErr Shm::read(void* buf, const size_t size) const noexcept {
        if (!m_valid || m_view == nullptr) return ShmOpErr::NotOpen;
        if (size > m_label_size) return ShmOpErr::TooLarge;

        std::memcpy(buf, data_ptr(), size);
        return ShmOpErr::None;
    }
    
    ShmOpErr Shm::write(const void* buf, const size_t size) noexcept {
        if (!m_valid || m_view == nullptr) return ShmOpErr::NotOpen;
        if (size > m_label_size) return ShmOpErr::TooLarge;

        std::memcpy(data_ptr(), buf, size);
        return ShmOpErr::None;
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

        m_valid = false;
    }
}