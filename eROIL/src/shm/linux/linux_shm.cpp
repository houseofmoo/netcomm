#if defined(EROIL_LINUX)
#include "shm/shm.h"

#include <fcntl.h>      // shm_open, O_*
#include <sys/mman.h>   // mmap, munmap, PROT_*, MAP_*
#include <sys/stat.h>   // mode_t
#include <unistd.h>     // ftruncate, close
#include <cerrno>

#include "shm/shm_header.h"
#include "types/types.h"

namespace eroil::shm {
    // static int as_native(shm_handle h) noexcept {
    //     return static_cast<int>(h);
    // }

    // static shm_handle from_native(int fd) noexcept {
    //     return static_cast<shm_handle>(fd);
    // }

    Shm::Shm(const Label label, const size_t label_size)
        : m_label(label), m_label_size(label_size), m_handle(-1), m_view(nullptr) {}

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

    std::string Shm::name() const noexcept {
        return "/eroil.label." + std::to_string(m_label);
    }

    ShmErr Shm::create() {
        if (is_valid()) return ShmErr::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            return ShmErr::InvalidName;
        }

        // O_EXCL makes create fail if it already exists
        m_handle = ::shm_open(n.c_str(), O_RDWR | O_CREAT | O_EXCL, 0777);
        if (m_handle < 0) {
            if (errno == EEXIST) return ShmErr::AlreadyExists;
            return ShmErr::UnknownError;
        }

        const size_t total = total_size();
        if (::ftruncate(fd, static_cast<off_t>(total)) != 0) {
            ::close(fd);
            ::shm_unlink(n.c_str()); // delete the partially created file
            return ShmErr::UnknownError;
        }

        m_view = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (m_view == MAP_FAILED) {
            ::close(fd);
            ::shm_unlink(n.c_str()); // delete the partially created file
            return ShmErr::FileMapFailed;
        }

        write_shm_header();
       
        return ShmErr::None;
    }

    ShmErr Shm::open() {
        if (is_valid()) return ShmErr::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            return ShmErr::InvalidName;
        }

        m_handle = ::shm_open(n.c_str(), O_RDWR, 0777);
        if (m_handle < 0) {
            if (errno == ENOENT) return ShmErr::DoesNotExist;
            return ShmErr::UnknownError;
        }

        const size_t total = total_size();

        m_view = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (m_view == MAP_FAILED) {
            ::close(m_handle);
            return ShmErr::FileMapFailed;
        }

        return validate_shm_header();
    }

    void Shm::close() noexcept {
        const size_t total = total_size();

        if (m_view != nullptr) {
            ::munmap(m_view, total);
            m_view = nullptr;
        }

        if (m_handle >= 0) {
            ::close(m_handle);
            m_handle = -1;
        }

        // delete the shared memory file
        //::shm_unlink(name().c_str());
    }
}

#endif