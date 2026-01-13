#include "shm/shm.h"
#include "windows_hdr.h"
#include <cstring>
#include <thread>
#include <chrono>
#include "shm/shm_header.h"
#include "types/types.h"
#include <eROIL/print.h>

namespace eroil::shm {
    static std::wstring to_windows_wstring(const std::string& s) {
        if (s.empty()) return {};

        int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (len <= 0) return {};

        std::wstring out((size_t)len, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
        return out;
    }

    std::byte* Shm::data_ptr() const noexcept {
        if (m_view == nullptr) return nullptr;
        return static_cast<std::byte*>(m_view) + sizeof(ShmHeader);
    }

    Shm::Shm(const Label label, const size_t label_size) : 
        m_label(label), m_label_size(label_size), m_handle(nullptr), m_view(nullptr) {}

    Shm::~Shm() {
        close();
    }

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

    size_t Shm::size_bytes_total() const noexcept {
        return m_label_size + sizeof(ShmHeader) + sizeof(LabelHeader);
    }

    size_t Shm::size_with_label_header() const noexcept {
        return  m_label_size + sizeof(LabelHeader);
    }

    ShmErr Shm::open_new() {
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
        auto* hdr = static_cast<ShmHeader*>(m_view);
        hdr->state.store(SHM_INITING, std::memory_order_relaxed);
        hdr->magic = MAGIC_NUM;
        hdr->version = VERSION;
        hdr->header_size = static_cast<uint16_t>(sizeof(ShmHeader));
        hdr->label_size = static_cast<uint32_t>(m_label_size);

        // zero data block
        std::memset(data_ptr(), 0, m_label_size);

        // publish readiness
        hdr->state.store(SHM_READY, std::memory_order_release);
 
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
        if (is_valid()) return ShmErr::DoubleOpen;

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
            if (e == ERROR_FILE_NOT_FOUND) return ShmErr::DoesNotExist;
            return ShmErr::UnknownError;
        }

        m_view = MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (m_view == nullptr) {
            close();
            m_handle = nullptr;
            return ShmErr::FileMapFailed;
        }

        // read header and validate
        const auto* hdr = static_cast<const ShmHeader*>(m_view);

        // wait for initialization
        constexpr uint32_t tries = 100;
        for (uint32_t i = 0; i < tries; ++i) {
            if (hdr->state.load(std::memory_order_acquire) == SHM_READY) break;
            ::Sleep(1); // yield to any ready thread and come back to me later
        }

        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            close();
            return ShmErr::NotInitialized; 
        }

        if (hdr->magic != MAGIC_NUM ||
            hdr->version != VERSION ||
            hdr->header_size != sizeof(ShmHeader) ||
            hdr->label_size != static_cast<uint32_t>(m_label_size)) {
            close();
            return ShmErr::LayoutMismatch;
        }

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

    ShmErr Shm::create_or_open(const uint32_t attempts, const uint32_t wait_ms) {
        if (is_valid()) return ShmErr::DoubleOpen;
        ShmErr err = ShmErr::None;

        for (uint32_t i = 0; i < attempts; ++i) {
            err = open_new();
            if (err == ShmErr::None) {
                return err;
            }

            if (err != ShmErr::AlreadyExists) {
                ERR_PRINT("shm::create_or_open() got unexpected error from open_new(): ", static_cast<int>(err));
                return err; // some other failure state
            }

            err = open_existing();
            if (err == ShmErr::None) return err;

            // wait and try again
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        }

        return err;
    }

    ShmOpErr Shm::read(void* buf, const size_t size) const noexcept {
        if (!is_valid()) return ShmOpErr::NotOpen;
        if (size > size_with_label_header()) return ShmOpErr::TooLarge;

        std::memcpy(buf, data_ptr(), size);
        return ShmOpErr::None;
    }
    
    ShmOpErr Shm::write(const void* buf, const size_t size) noexcept {
        if (!is_valid()) return ShmOpErr::NotOpen;
        if (size > size_with_label_header()) return ShmOpErr::TooLarge;

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
    }
}