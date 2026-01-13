#pragma once
#include <string>
#include <vector>
#include <atomic>
#include "events/named_event.h"
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
        private:
            std::byte* data_ptr() const noexcept;

        protected:
            Label m_label;
            size_t m_label_size;
            shm_handle m_handle;
            shm_view m_view;

        public:
            Shm(const Label label, const size_t label_size);
            virtual ~Shm();

            // do not copy
            Shm(const Shm&) = delete;
            Shm& operator=(const Shm&) = delete;

            // allow move
            Shm(Shm&& other) noexcept;
            Shm& operator=(Shm&& other) noexcept;
            
            bool is_valid() const noexcept;
            std::string name() const noexcept;
            size_t size_bytes_total() const noexcept;
            size_t size_with_label_header() const noexcept;
            ShmErr create();
            ShmErr open();
            ShmErr create_or_open(const uint32_t attempts = 10, const uint32_t wait_ms = 100);
            ShmOpErr read(void* buf, const size_t size) const noexcept;
            ShmOpErr write(const void* buf, const size_t size) noexcept;
            void close() noexcept;

            template <typename T>
            T* map_to_data() const { return reinterpret_cast<T*>(data_ptr()); }
    };
}