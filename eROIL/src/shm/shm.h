#pragma once
#include <string>
#include <vector>
#include <atomic>
#include "types/const_types.h"
#include "macros.h"

namespace eroil::shm {
    enum class ShmErr {
        None = 0,
        DoubleOpen,
        InvalidName,
        AlreadyExists,
        DoesNotExist,
        FileMapFailed,
        SizeMismatch,
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

    enum class ShmOp {
        Create,
        Open,
        Read,
        Write,
    };

    struct ShmResult {
        ShmErr code;
        ShmOp op;

        bool ok() const noexcept {
            return code == ShmErr::None;
        }

        std::string_view code_to_string() const noexcept {
            switch (code) {
                case ShmErr::None: return "None";
                case ShmErr::DoubleOpen: return "DoubleOpen";
                case ShmErr::InvalidName: return "InvalidName";
                case ShmErr::AlreadyExists: return "AlreadyExists";
                case ShmErr::DoesNotExist: return "DoesNotExist";
                case ShmErr::FileMapFailed: return "FileMapFailed";
                case ShmErr::SizeMismatch: return "SizeMismatch";
                case ShmErr::NotInitialized: return "NotInitialized";
                case ShmErr::UnknownError: return "UnknownError";

                case ShmErr::TooLarge: return "TooLarge";
                case ShmErr::NotOpen: return "NotOpen";
                case ShmErr::DuplicateSendEvent: return "DuplicateSendEvent";
                case ShmErr::AddSendEventError: return "AddSendEventError";
                case ShmErr::SetRecvEventError: return "SetRecvEventError";
                case ShmErr::RecvFailed: return "RecvFailed";
                case ShmErr::WouldBlock: return "WouldBlock";
                case ShmErr::InvalidOffset: return "InvalidOffset";
                default: return "Unknown - error is undefined";
            }
        }

        std::string_view op_to_string() const noexcept {
            switch (op) {
                case ShmOp::Create: return "Create";
                case ShmOp::Open: return "Open";
                case ShmOp::Read: return "Read";
                case ShmOp::Write: return "Write";
                default: return "Unknown - op is undefined";
            }
        }
    };

    class Shm {
        private:
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
            NO_DISCARD ShmResult create();
            NO_DISCARD ShmResult open();
            void close() noexcept;

            // shared implementation
            void memset(size_t offset, int32_t val, size_t bytes);
            size_t total_size() const noexcept;
            NO_DISCARD ShmResult read(void* buf, const size_t size, const size_t offset) const noexcept;
            NO_DISCARD ShmResult write(const void* buf, const size_t size, const size_t offset) noexcept;
            template <typename T>
            T* map_to_type(size_t offset) const { 
                if (offset > m_total_size) return nullptr;
                if (sizeof(T) > (m_total_size - offset)) return nullptr;

                return reinterpret_cast<T*>(
                    static_cast<std::byte*>(m_view) + offset
                ); 
            }
    };
}