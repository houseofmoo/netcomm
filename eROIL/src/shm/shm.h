#pragma once
#include <string>
#include <vector>
#include <atomic>
#include "types/types.h"

namespace eroil::shm {
    enum class ShmErr {
        None,
        DoubleOpen,
        InvalidName,
        AlreadyExists,
        DoesNotExist,
        FileMapFailed,
        LayoutMismatch,
        NotInitialized,
        UnknownError,
    };

    enum class ShmOpErr {
        None,
        TooLarge,
        NotOpen,
        DuplicateSendEvent,
        AddSendEventError,
        SetRecvEventError,
        RecvFailed,
        WouldBlock,
        InvalidOffset,
    };

    class Shm {
        protected:
            int32_t m_id;
            size_t m_total_size;
            shm_handle m_handle;
            shm_view m_view;

        public:
            Shm(const int32_t id, const size_t total_size);
            virtual ~Shm() { close(); }

            // do not copy
            Shm(const Shm&) = delete;
            Shm& operator=(const Shm&) = delete;

            // allow move
            Shm(Shm&& other) noexcept;
            Shm& operator=(Shm&& other) noexcept;
            
            // platform dependent
            bool is_valid() const noexcept;
            std::string name() const noexcept;
            ShmErr create();
            ShmErr open();
            void close() noexcept;

            // shared implementation
            template <typename T>
            T* map_to_type(size_t offset) const;
            void memset(size_t offset, int32_t val, size_t bytes);
            size_t total_size() const noexcept;

            ShmOpErr read(void* buf, const size_t size, const size_t offset) const noexcept;
            ShmOpErr write(const void* buf, const size_t size, const size_t offset) noexcept;
            
            // on open/create validation
            // void write_header_init();
            // void write_header_ready();
            // ShmErr validate_shm_header();
            // bool is_block_ready();
            
        private:
            ShmErr create_or_open(const uint32_t attempts = 10, const uint32_t wait_ms = 100);
            // std::byte* data_ptr() const noexcept;
            // void write_shm_header();
            // void write_header_init();
            // void writer_header_ready();
            // ShmErr validate_shm_header();
    };
}