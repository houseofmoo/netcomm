#pragma once
#include <string>
#include <vector>
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
        UnknownError,
    };

    enum class ShmOpErr {
        None,
        TooLarge,
        NotOpen,
        DuplicateSendEvent, // send event already exists
        AddSendEventError,  // could not add send event
        SetRecvEventError,  // could not add send event
        RecvFailed,
        WouldBlock,
    };

    static constexpr uint32_t kShmMagic   = 0x314D4853u; // 'SHM1'
    static constexpr uint16_t kShmVersion = 1;

    #pragma pack(push, 1)
    struct ShmHeader {
        uint32_t magic;        // 'SHM1' little-endian
        uint16_t version;      // bump when layout changes
        uint16_t header_size;  // sizeof(ShmHeader)
        uint32_t label_size;    // size of data stored here
    };
    #pragma pack(pop)

    bool shm_exists(std::string name);

    class Shm {
        private:
            std::byte* data_ptr() const noexcept;

        protected:
            Label m_label;
            size_t m_label_size;
            shm_handle m_handle;
            shm_view m_view;
            bool m_valid;

        public:
            Shm(const Label label, const size_t label_size);
            virtual ~Shm();
            
            std::string name() const noexcept;
            size_t size_bytes_total() const noexcept;
            ShmErr open_new();
            ShmErr open_new_rety(const uint32_t attempts = 5, const uint32_t wait_ms = 100);
            ShmErr open_existing();
            ShmErr open_existing_rety(const uint32_t attempts = 5, const uint32_t wait_ms = 100);
            ShmOpErr read(void* buf, const size_t size) const noexcept;
            ShmOpErr write(const void* buf, const size_t size) noexcept;
            virtual void close() noexcept;

            template <typename T>
            T* map_to_data() const { return reinterpret_cast<T*>(data_ptr()); }
    };

    class ShmSend : public Shm {
        private:
            std::vector<evt::NamedEvent> m_send_events;

        public:
            ShmSend(const Label label, const size_t label_size);
            ~ShmSend() override;

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
            ~ShmRecv() override;

            ShmOpErr set_recv_event(NodeId my_id);
            bool has_recv_event(const Label label, const NodeId my_id) const;
            ShmOpErr recv(void* buf, const size_t size);
            ShmOpErr recv_nonblocking(void* buf, const size_t size);
    };
}