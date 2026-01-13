#pragma once
#include <cstddef>
#include <cstdint>
#include <array>

namespace eroil {
    // UNIVERSAL AND PLATFORM SPECIFIC TYPE NAMES

    using Label = std::int32_t;
    using NodeId = std::int32_t;
    using handle_uid = std::uint64_t;

    static constexpr Label INVALID_LABEL = -1;
    static constexpr NodeId INVALID_NODE = -1;

    constexpr std::uint32_t MAX_LABELS = 200;
    static constexpr std::uint32_t MAGIC_NUM = 0x4C4F5245u; // 'EROL' as ascii bytes
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t SOCKET_DATA_MAX_SIZE = 1u << 20; // 1 MB
    
    // using u8 = std::uint8_t;
    // using u16 = std::uint16_t;
    // using u32 = std::uint32_t;
    // using u64 = std::uint64_t;
    
    // using i8 = std::int8_t;
    // using i16 = std::int16_t;
    // using i32 = std::int32_t;
    // using i64 = std::int64_t;
    
    #if defined(EROIL_WIN32)
        using shm_handle = void*;
        using sem_handle = void*;
        using shm_view = void*;
        using socket_handle = std::uintptr_t;
    #elif defined(EROIL_LINUX)
        struct sem_t; // forward decl so we dont have to include <semaphore.h>
        using shm_handle = int;
        using sem_handle = sem_t*;
        using shm_view = void*;
        using socket_handle = int;
    #else
        #error "Must define EROIL_LINUX or EROIL_WIN32"
    #endif

    struct LabelInfo {
        Label label;
        std::uint32_t size;
    };

    struct BroadcastMessage {
        std::int32_t id;
        std::array<LabelInfo, MAX_LABELS> send_labels;
        std::array<LabelInfo, MAX_LABELS> recv_labels;
    };

    struct LabelHeader {
        std::uint32_t magic;
        std::uint16_t version;
        std::int32_t source_id;
        std::uint16_t flags;
        std::int32_t label;
        std::uint32_t data_size;
    };

    enum class LabelFlag : std::uint16_t {
        Data = 1 << 0,
        Connect = 1 << 1,
        Disconnect = 1 << 2,
        Ping = 1 << 3,
    };

    inline void set_flag(std::uint16_t& flags, const LabelFlag flag) { flags |= static_cast<std::uint16_t>(flag); }
    inline bool has_flag(const std::uint16_t flags, const LabelFlag flag) { return flags & static_cast<std::uint16_t>(flag); }
}