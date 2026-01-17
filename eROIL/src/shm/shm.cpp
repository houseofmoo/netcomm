#include "shm.h"
#include <thread>
#include <chrono>
#include <eROIL/print.h>
#include "shm_header.h"
#include <cstring>

namespace eroil::shm {
    template <typename T>
    T* Shm::map_to_data() const { 
        return reinterpret_cast<T*>(data_ptr()); 
    }

    size_t Shm::total_size() const noexcept { 
        return size_with_header() + sizeof(ShmHeader); 
    }

    size_t Shm::size_with_header() const noexcept { 
        return m_label_size + sizeof(LabelHeader); 
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
        if (size > size_with_header()) return ShmOpErr::TooLarge;

        std::memcpy(buf, data_ptr(), size);
        return ShmOpErr::None;
    }

    ShmOpErr Shm::write(const void* buf, const size_t size) noexcept {
        if (!is_valid()) return ShmOpErr::NotOpen;
        if (size > size_with_header()) return ShmOpErr::TooLarge;

        std::memcpy(data_ptr(), buf, size);
        return ShmOpErr::None;
    }

    std::byte* Shm::data_ptr() const noexcept {
        if (m_view == nullptr) return nullptr;
        return static_cast<std::byte*>(m_view) + sizeof(ShmHeader);
    }

    void Shm::write_shm_header() {
        // write header to block
        auto* hdr = static_cast<ShmHeader*>(m_view);
        hdr->state.store(SHM_INITING, std::memory_order_relaxed);
        hdr->magic = MAGIC_NUM;
        hdr->version = VERSION;
        hdr->header_size = static_cast<uint16_t>(sizeof(ShmHeader));
        hdr->data_size = static_cast<uint32_t>(size_with_header());

        // zero data block
        std::memset(data_ptr(), 0, size_with_header());

        // publish readiness
        hdr->state.store(SHM_READY, std::memory_order_release);
    }
}