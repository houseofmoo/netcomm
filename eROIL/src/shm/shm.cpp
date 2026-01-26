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

    ShmOpErr Shm::read(void* buf, const size_t size, const size_t offset) const noexcept {
        if (!is_valid()) return ShmOpErr::NotOpen;
        if (size > m_total_size) return ShmOpErr::TooLarge;
        if (offset + size > m_total_size) return ShmOpErr::InvalidOffset;

        std::memcpy(buf, static_cast<std::byte*>(m_view) + offset, size);
        return ShmOpErr::None;
    }

    ShmOpErr Shm::write(const void* buf, const size_t size, const size_t offset) noexcept {
        if (!is_valid()) return ShmOpErr::NotOpen;
        if (size > m_total_size) return ShmOpErr::TooLarge;
        if (offset + size > m_total_size) return ShmOpErr::InvalidOffset;

        std::memcpy(static_cast<std::byte*>(m_view) + offset, buf, size);
        return ShmOpErr::None;
    }
}