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
    };

    class Shm {
        protected:
            Label m_label;
            size_t m_label_size;
            shm_handle m_handle;
            shm_view m_view;

        public:
            Shm(const Label label, const size_t label_size);
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
            T* map_to_data() const;
            size_t total_size() const noexcept;
            size_t size_with_header() const noexcept;
            ShmErr create_or_open(const uint32_t attempts = 10, const uint32_t wait_ms = 100);
            ShmOpErr read(void* buf, const size_t size) const noexcept;
            ShmOpErr write(const void* buf, const size_t size) noexcept;

        private:
            std::byte* data_ptr() const noexcept;
            void write_shm_header();
            ShmErr validate_shm_header();
    };
}