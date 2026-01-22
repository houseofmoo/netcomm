#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include "assertion.h"

namespace eroil {
    using Label = std::int32_t;
    using NodeId = std::int32_t;
    using handle_uid = std::uint64_t;

    static constexpr Label INVALID_LABEL = -1;
    static constexpr NodeId INVALID_NODE = -1;

    static constexpr std::uint32_t MAX_LABELS = 200;
    static constexpr std::uint32_t MAGIC_NUM = 0x4C4F5245u; // 'EROL' as ascii bytes
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t SOCKET_DATA_MAX_SIZE = 1u << 20; // 1 MB

    using std::uint8_t;
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    
    using std::int8_t;
    using std::int16_t;
    using std::int32_t;
    using std::int64_t;

    using std::uintptr_t;
    using std::intptr_t;
    using std::size_t;
    
    // using u8 = std::uint8_t;
    // using u16 = std::uint16_t;
    // using u32 = std::uint32_t;
    // using u64 = std::uint64_t;
    
    // using i8 = std::int8_t;
    // using i16 = std::int16_t;
    // using i32 = std::int32_t;
    // using i64 = std::int64_t;

    // using uptr = std::uintptr_t;
    // using iptr = std::intptr_t;
    // using usize = std::size_t;

    #if defined(EROIL_LINUX)
        constexpr std::int32_t INVALID_SOCKET = -1;
        using shm_handle = int;
        using shm_view = void*;
        using sem_handle = void*;
        using socket_handle = int;
    #elif defined(EROIL_WIN32)
        using shm_handle = void*;
        using shm_view = void*;
        using sem_handle = void*;
        using socket_handle = std::uintptr_t;
    #else
        #error "Must define EROIL_LINUX or EROIL_WIN32"
    #endif

    namespace io {
        struct LabelInfo {
            Label label = INVALID_LABEL;
            std::uint32_t size = 0;

            LabelInfo() noexcept = default;
            bool operator<(const LabelInfo& other) const noexcept {
                return label < other.label;
            }
        };

        struct LabelsSnapshot {
            std::uint64_t gen = 0;
            std::array<LabelInfo, MAX_LABELS> labels{};
        };

        struct BroadcastMessage {
            std::int32_t id = INVALID_NODE;
            LabelsSnapshot send_labels{};
            LabelsSnapshot recv_labels{};
        };

        struct LabelHeader {
            std::uint32_t magic = 9;
            std::uint16_t version = 0;
            std::int32_t source_id = INVALID_LABEL;
            std::uint16_t flags = 0;
            std::int32_t label = INVALID_LABEL;
            std::uint32_t data_size = 0;
            std::uint32_t recv_offset = 0;
        };

        enum class LabelFlag : std::uint16_t {
            Data = 1 << 0,
            Connect = 1 << 1,
            Disconnect = 1 << 2,
            Ping = 1 << 3,
        };

        struct SendBuf {
            void* data_src_addr = nullptr; // where the data was copied from (for send IOSB)
            std::unique_ptr<std::uint8_t[]> data = nullptr;
            std::size_t data_size = 0;
            std::size_t total_size = 0;  // size of data + header

            SendBuf(void* src_buf, const std::size_t size) : data_src_addr(src_buf) {
                DB_ASSERT(data_src_addr != nullptr, "cannot have nullptr src addr for send label");
                DB_ASSERT(size != 0, "cannot have 0 data size for send label");

                data_size = size;
                total_size = size + sizeof(LabelHeader);
                data = std::make_unique<std::uint8_t[]>(total_size);
            }
                
            // move ok
            SendBuf(SendBuf&&) noexcept = default;
            SendBuf& operator=(SendBuf&&) noexcept = default;

            // copy deleted
            SendBuf(const SendBuf&) = delete;
            SendBuf& operator=(const SendBuf&) = delete;
        };

        // helper funcs
        inline void set_flag(std::uint16_t& flags, const LabelFlag flag) { flags |= static_cast<std::uint16_t>(flag); }
        inline bool has_flag(const std::uint16_t flags, const LabelFlag flag) { return flags & static_cast<std::uint16_t>(flag); }
    }
}