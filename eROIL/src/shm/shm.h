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
            // bool m_valid;

        public:
            Shm(const Label label, const size_t label_size);
            virtual ~Shm();

            Shm(const Shm&) = delete;
            Shm& operator=(const Shm&) = delete;

            Shm(Shm&& other) noexcept;
            Shm& operator=(Shm&& other) noexcept;
            
            bool is_valid() const noexcept;
            std::string name() const noexcept;
            size_t size_bytes_total() const noexcept;
            ShmErr open_new();
            ShmErr open_new_rety(const uint32_t attempts = 5, const uint32_t wait_ms = 100);
            ShmErr open_existing();
            ShmErr open_existing_rety(const uint32_t attempts = 5, const uint32_t wait_ms = 100);
            ShmErr create_or_open(const uint32_t attempts = 10, const uint32_t wait_ms = 100);
            ShmOpErr read(void* buf, const size_t size) const noexcept;
            ShmOpErr write(const void* buf, const size_t size) noexcept;
            void close() noexcept;

            template <typename T>
            T* map_to_data() const { return reinterpret_cast<T*>(data_ptr()); }
    };

    class ShmSend : public Shm {
        private:
            std::vector<evt::NamedEvent> m_send_events;

        public:
            ShmSend(const Label label, const size_t label_size);
            ~ShmSend() = default;

            ShmSend(const ShmSend&) = delete;
            ShmSend& operator=(const ShmSend&) = delete;

            ShmSend(ShmSend&& other) noexcept = default;
            ShmSend& operator=(ShmSend&& other) noexcept = default;

            ShmOpErr add_send_event(const NodeId to_id);
            bool has_send_event(const Label label, const NodeId to_id) const;
            void remove_send_event(const NodeId to_id);
            ShmOpErr send(const void* buf, const size_t size);
    };

    class ShmRecv : public Shm {
        private:
            evt::NamedEvent m_recv_event;

        public:
            ShmRecv(const Label label, const size_t label_size);
            ~ShmRecv() = default;

            ShmRecv(const ShmRecv&) = delete;
            ShmRecv& operator=(const ShmRecv&) = delete;

            ShmRecv(ShmRecv&& other) noexcept = default;
            ShmRecv& operator=(ShmRecv&& other) noexcept = default;

            ShmOpErr set_recv_event(NodeId my_id);
            bool has_recv_event(const Label label, const NodeId my_id) const;
            ShmOpErr recv(void* buf, const size_t size);
            ShmOpErr recv_nonblocking(void* buf, const size_t size);
            void interrupt_wait();
    };
}