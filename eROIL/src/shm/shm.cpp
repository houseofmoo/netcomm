#include "shm.h"
#include <thread>
#include <chrono>
#include <eROIL/print.h>
#include <cstring>

namespace eroil::shm {
    void Shm::memset(size_t offset, int32_t val, size_t bytes) {
        if (offset+ bytes > m_total_size) return;
        std::memset(
            static_cast<std::byte*>(m_view) + offset, 
            static_cast<int>(val), 
            bytes
        );
    }

    size_t Shm::total_size() const noexcept { 
        return m_total_size; 
    }

    ShmResult Shm::read(void* buf, const size_t size, const size_t offset) const noexcept {
        if (!is_valid()) return { ShmErr::NotOpen, ShmOp::Read };
        if (size > m_total_size) return { ShmErr::TooLarge, ShmOp::Read };
        if (offset + size > m_total_size) return { ShmErr::InvalidOffset, ShmOp::Read };

        std::memcpy(buf, static_cast<std::byte*>(m_view) + offset, size);
        return { ShmErr::None, ShmOp::Read };
    }

    ShmResult Shm::write(const void* buf, const size_t size, const size_t offset) noexcept {
        if (!is_valid()) return { ShmErr::NotOpen, ShmOp::Write };
        if (size > m_total_size) return{ ShmErr::TooLarge, ShmOp::Write };
        if (offset + size > m_total_size) return { ShmErr::InvalidOffset, ShmOp::Write };

        std::memcpy(static_cast<std::byte*>(m_view) + offset, buf, size);
        return { ShmErr::None, ShmOp::Write };
    }
}