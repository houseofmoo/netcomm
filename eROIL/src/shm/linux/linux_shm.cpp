#if defined(EROIL_LINUX)
#include "shm/shm.h"

#include <fcntl.h>      // shm_open, O_*
#include <sys/mman.h>   // mmap, munmap, PROT_*, MAP_*
#include <sys/stat.h>   // mode_t
#include <unistd.h>     // ftruncate, close
#include <cerrno>

#include "shm/shm_header.h"
#include "types/const_types.h"

namespace eroil::shm {
    // static int as_native(shm_handle h) noexcept {
    //     return static_cast<int>(h);
    // }

    // static shm_handle from_native(int fd) noexcept {
    //     return static_cast<shm_handle>(fd);
    // }

    Shm::Shm(const int32_t id, const size_t total_size) : 
        m_id(id), m_total_size(total_size), m_handle(-1), m_view(nullptr) {}

    Shm::Shm(Shm&& other) noexcept
        : m_id(other.m_id),
          m_total_size(other.m_total_size),
          m_handle(other.m_handle),
          m_view(other.m_view) {

        other.m_handle = -1;
        other.m_view   = nullptr;
        other.m_id  = -1;
        other.m_total_size = 0;
    }

    Shm& Shm::operator=(Shm&& other) noexcept {
        if (this != &other) {
            close();

            m_id = other.m_id;
            m_total_size = other.m_total_size;
            m_handle = other.m_handle;
            m_view = other.m_view;

            other.m_handle = -1;
            other.m_view = nullptr;
            other.m_id = -1;
            other.m_total_size = 0;
        }
        return *this;
    }

    bool Shm::is_valid() const noexcept {
        return m_handle >= 0 && m_view != nullptr;
    }

    std::string Shm::name() const noexcept {
        return "/eroil.node." + std::to_string(m_id);
    }

    ShmResult::Code Shm::create() {
        if (is_valid()) return ShmResult::Code::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            return ShmResult::Code::InvalidName;
        }

        // O_EXCL makes create fail if it already exists
        m_handle = ::shm_open(n.c_str(), O_RDWR | O_CREAT | O_EXCL, 0777);
        if (m_handle < 0) {
            if (errno == EEXIST) return ShmResult::Code::AlreadyExists;
            return ShmResult::Code::UnknownError;
        }

        const size_t total = total_size();
        if (::ftruncate(m_handle, static_cast<off_t>(total)) != 0) {
            ::close(m_handle);
            ::shm_unlink(n.c_str()); // delete the partially created file
            return ShmResult::Code::UnknownError;
        }

        m_view = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, m_handle, 0);
        if (m_view == MAP_FAILED) {
            ::close(m_handle);
            ::shm_unlink(n.c_str()); // delete the partially created file
            return ShmResult::Code::FileMapFailed;
        }
       
        return ShmResult::Code::None;
    }

    ShmResult::Code Shm::open() {
        if (is_valid()) return ShmResult::Code::DoubleOpen;

        const std::string n = name();
        if (n.empty() || n[0] != '/') {
            return ShmResult::Code::InvalidName;
        }

        m_handle = ::shm_open(n.c_str(), O_RDWR, 0777);
        if (m_handle < 0) {
            if (errno == ENOENT) return ShmResult::Code::DoesNotExist;
            return ShmResult::Code::UnknownError;
        }

        const size_t total = total_size();

        m_view = ::mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, m_handle, 0);
        if (m_view == MAP_FAILED) {
            ::close(m_handle);
            return ShmResult::Code::FileMapFailed;
        }

        return ShmResult::Code::None;
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