#include "shm.h"
#include <thread>
#include <chrono>
#include <eROIL/print.h>
#include "shm_header.h"
#include <cstring>

namespace eroil::shm {
    template <typename T>
    T* Shm::map_to_type(size_t offset) const { 
        if (offset > m_total_size) return nullptr;

        return reinterpret_cast<T*>(
            static_cast<std::byte*>(m_view) + offset
        ); 
    }

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

    // size_t Shm::size_minus_shm_header() const noexcept { 
    //     return m_total_size - sizeof(ShmHeader); 
    // }

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

    // std::byte* Shm::data_ptr() const noexcept {
    //     if (m_view == nullptr) return nullptr;
    //     return static_cast<std::byte*>(m_view) + sizeof(ShmHeader);
    // }

    // void Shm::write_header_init() {
    //     // write header to block
    //     auto* hdr = static_cast<ShmHeader*>(m_view);
    //     hdr->state.store(SHM_INITING, std::memory_order_relaxed);
    //     hdr->magic = MAGIC_NUM;
    //     hdr->version = VERSION;
    //     hdr->total_size = static_cast<uint32_t>(total_size());

    //     // zero data block
    //     std::memset(
    //         static_cast<std::byte*>(m_view) + sizeof(ShmHeader), 
    //         0, 
    //         size_minus_shm_header()
    //     );

    //     // publish readiness
    //     hdr->state.store(SHM_READY, std::memory_order_release);
    // }

    // void Shm::write_header_init() {
    //     // announce this block is being initialized
    //     auto* hdr = static_cast<ShmHeader*>(m_view);
    //     hdr->state.store(SHM_INITING, std::memory_order_relaxed);
    //     hdr->magic = MAGIC_NUM;
    //     hdr->version = VERSION;
    //     hdr->total_size = static_cast<uint32_t>(total_size());
    // }

    // void Shm::write_header_ready() {
    //     // after init complete, publish ready
    //     auto* hdr = static_cast<ShmHeader*>(m_view);
    //     hdr->state.store(SHM_READY, std::memory_order_release);
    // }

    // ShmErr Shm::validate_shm_header() {
    //     const auto* hdr = static_cast<const ShmHeader*>(m_view);

    //     constexpr uint32_t tries = 100;
    //     for (uint32_t i = 0; i < tries; ++i) {
    //         if (hdr->state.load(std::memory_order_acquire) == SHM_READY) break;
    //         std::this_thread::sleep_for(std::chrono::milliseconds(1));
    //     }

    //     if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
    //         close();
    //         return ShmErr::NotInitialized;
    //     }

    //     if (hdr->magic != MAGIC_NUM ||
    //         hdr->version != VERSION ||
    //         hdr->total_size != static_cast<uint32_t>(total_size())) {
    //         close();
    //         return ShmErr::LayoutMismatch;
    //     }

    //     return ShmErr::None;
    // }

    // bool Shm::is_block_ready() {
    //     auto* hdr = static_cast<ShmHeader*>(m_view);
    //     constexpr uint32_t tries = 100;
    //     for (uint32_t i = 0; i < tries; ++i) {
    //         if (hdr->state.load(std::memory_order_acquire) == SHM_READY) break;
    //         std::this_thread::sleep_for(std::chrono::milliseconds(1));
    //     }

    //     if (hdr->state.load(std::memory_order_acquire) != SHM_READY) {
    //         return false;
    //     }
    //     return true;
    // }


}