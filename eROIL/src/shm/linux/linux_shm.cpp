#ifdef EROIL_LINUX

// If you hit "timespec doesn't exist" style issues in your build, you can add:
// #define _POSIX_C_SOURCE 200809L
// before system headers (or add as a compile definition).

#include "shm/shm.h"

#include <fcntl.h>      // shm_open, O_*
#include <sys/mman.h>   // mmap, munmap, PROT_*, MAP_*
#include <sys/stat.h>   // mode_t
#include <unistd.h>     // ftruncate, close
#include <cerrno>
#include <cstring>
#include <thread>
#include <chrono>

#include "shm/shm_header.h"
#include "types/types.h"
#include <eROIL/print.h>

namespace eroil::shm {
    std::string Shm::name() const noexcept {
        return "/eroil.label." + std::to_string(m_label);
    }

    static int as_native(shm_handle h) noexcept {
        return static_cast<int>(h);
    }

    static shm_handle from_native(int fd) noexcept {
        return static_cast<shm_handle>(fd);
    }

    std::byte* Shm::data_ptr() const noexcept {
        if (m_view == nullptr) return nullptr;
        return static_cast<std::byte*>(m_view) + sizeof(ShmHeader);
    }

    Shm::Shm(const Label label, const size_t label_size)
        : m_label(label), m_label_size(label_size), m_handle(-1), m_view(nullptr) {}

    Shm::~Shm() {
        close();
    }

    Shm::Shm(Shm&& other) noexcept
        : m_label(other.m_label),
          m_label_size(other.m_label_size),
          m_handle(other.m_handle),
          m_view(other.m_view) {

        other.m_handle = -1;
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

            other.m_handle = -1;
            other.m_view = nullptr;
            other.m_label = 0;
            other.m_label_size = 0;
        }
        return *this;
    }

    bool Shm::is_valid() const noexcept {
        return m_handle >= 0 && m_view != nullptr;
    }

    size_t Shm::size_bytes_total() const noexcept {
        return m_label_size + sizeof(ShmHeader) + sizeof(LabelHeader);
    }

    size_t Shm::size_with_label_header() const noexcept {
        return m_label_size + sizeof(LabelHeader);
    }

    ShmErr Shm::create() {
        if (is_valid()) return ShmErr::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            return ShmErr::InvalidName;
        }

        // O_EXCL makes create fail if it already exists
        int fd = ::shm_open(n.c_str(), O_RDWR | O_CREAT | O_EXCL, 0777);
        if (fd < 0) {
            if (errno == EEXIST) return ShmErr::AlreadyExists;
            return ShmErr::UnknownError;
        }

        const size_t total = size_bytes_total();
        if (::ftruncate(fd, static_cast<off_t>(total)) != 0) {
            ::close(fd);
            ::shm_unlink(n.c_str()); // delete the partially created file
            return ShmErr::UnknownError;
        }

        void* view = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (view == MAP_FAILED) {
            ::close(fd);
            ::shm_unlink(n.c_str()); // delete the partially created file
            return ShmErr::FileMapFailed;
        }

        m_handle = from_native(fd);
        m_view   = view;

        // write header to block
        auto* hdr = static_cast<ShmHeader*>(m_view);
        hdr->state.store(SHM_INITING, std::memory_order_relaxed);
        hdr->magic = MAGIC_NUM;
        hdr->version = VERSION;
        hdr->header_size = static_cast<uint16_t>(sizeof(ShmHeader));
        hdr->data_size = static_cast<uint32_t>(size_with_label_header());

        // zero data block
        std::memset(data_ptr(), 0, size_with_label_header());

        // publish readiness
        hdr->state.store(SHM_READY, std::memory_order_release);

        return ShmErr::None;
    }

    ShmErr Shm::open() {
        if (is_valid()) return ShmErr::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            return ShmErr::InvalidName;
        }

        int fd = ::shm_open(n.c_str(), O_RDWR, 0777);
        if (fd < 0) {
            if (errno == ENOENT) return ShmErr::DoesNotExist;
            return ShmErr::UnknownError;
        }

        const size_t total = size_bytes_total();

        void* view = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (view == MAP_FAILED) {
            ::close(fd);
            return ShmErr::FileMapFailed;
        }

        m_handle = from_native(fd);
        m_view = view;

        // read header and validate
        const auto* hdr = static_cast<const ShmHeader*>(m_view);

        constexpr uint32_t tries = 100;
        for (uint32_t i = 0; i < tries; ++i) {
            if (hdr->state.load(std::memory_order_acquire) == SHM_READY) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
            close();
            return ShmErr::NotInitialized;
        }

        if (hdr->magic != MAGIC_NUM ||
            hdr->version != VERSION ||
            hdr->header_size != sizeof(ShmHeader) ||
            hdr->data_size != static_cast<uint32_t>(size_with_label_header())) {
            close();
            return ShmErr::LayoutMismatch;
        }

        return ShmErr::None;
    }

    ShmErr Shm::create_or_open(const uint32_t attempts, const uint32_t wait_ms) {
        if (is_valid()) return ShmErr::DoubleOpen;

        ShmErr err = ShmErr::None;
        for (uint32_t i = 0; i < attempts; ++i) {
            err = create();
            if (err == ShmErr::None) return err;

            if (err != ShmErr::AlreadyExists) {
                ERR_PRINT("shm::create_or_open() got unexpected error from create(): ", static_cast<int>(err));
                return err;
            }

            err = open();
            if (err == ShmErr::None) return err;

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
        const size_t total = size_bytes_total();

        if (m_view != nullptr) {
            ::munmap(m_view, total);
            m_view = nullptr;
        }

        if (m_handle >= 0) {
            ::close(as_native(m_handle));
            m_handle = -1;
        }

        // delete the shared memory file
        //::shm_unlink(name().c_str());
    }
}

#endif
