#pragma once
#include <string>
#include <vector>
#include <atomic>
#include "types/const_types.h"
#include "types/macros.h"

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

        // operations
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

            EROIL_NO_COPY(Shm)
            EROIL_DECL_MOVE(Shm)
            
            // platform dependent
            bool is_valid() const noexcept;
            std::string name() const noexcept;
            ShmErr create();
            ShmErr open();
            void close() noexcept;

            // shared implementation
            void memset(size_t offset, int32_t val, size_t bytes);
            size_t total_size() const noexcept;
            ShmErr read(void* buf, const size_t size, const size_t offset) const noexcept;
            ShmErr write(const void* buf, const size_t size, const size_t offset) noexcept;
            template <typename T>
            T* map_to_type(size_t offset) const { 
                if (offset > m_total_size) return nullptr;

                return reinterpret_cast<T*>(
                    static_cast<std::byte*>(m_view) + offset
                ); 
            }
            
        private:
            ShmErr create_or_open(const uint32_t attempts = 10, const uint32_t wait_ms = 100);
    };
}