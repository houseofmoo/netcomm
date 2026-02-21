#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "assertion.h"

namespace eroil {
    constexpr std::string_view MANAGE_CONFIG_FILE_PATH = "etc/manager.cfg";
    constexpr std::string_view PEER_IP_FILE_PATH = "etc/peer_ips.cfg";
    constexpr std::string_view LOCAL_HOST = "127.0.0.1";
    constexpr std::uint16_t PORT_START = 8080;

    using Label = std::int32_t;
    using NodeId = std::int32_t;
    using handle_uid = std::uint64_t;

    static constexpr Label INVALID_LABEL = -1;
    static constexpr NodeId INVALID_NODE = -1;

    static constexpr std::uint32_t MAX_LABELS = 200;
    static constexpr std::uint32_t MAGIC_NUM = 0x4C4F5245u; // 'EROL' as ascii bytes
    static constexpr std::uint16_t VERSION = 1;

    static constexpr std::size_t KILOBYTE = 1024u;
    static constexpr std::size_t MEGABYTE = 1024u * KILOBYTE;

    static constexpr std::size_t MAX_LABEL_SIZE = 1 * MEGABYTE;
    static_assert(MAX_LABEL_SIZE % 64 == 0);

    static constexpr std::size_t SHM_BLOCK_SIZE = 128 * MEGABYTE;
    static_assert(SHM_BLOCK_SIZE % 64 == 0);


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
}