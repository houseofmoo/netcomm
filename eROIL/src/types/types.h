#pragma once
#include <cstddef>
#include <cstdint>

namespace eroil {
    // UNIVERSAL AND PLATFORM SPECIFIC TYPE NAMES

    constexpr std::uint32_t MAX_LABELS = 200;
    static constexpr std::uint32_t MAGIC_NUM = 0x4C4F5245u; // 'EROL'
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t SOCKET_DATA_MAX_SIZE = 1u << 20; // 1 MB

    using Label = std::int32_t;
    using NodeId = std::int32_t;
    using handle_uid = std::uint64_t;
    
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    
    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    
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
}