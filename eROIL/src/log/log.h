#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace eroil::elog {
    // this whole logging system is designed to be as fast as possible with minimal overhead
    // so we use a fixed size ring buffer and avoid any dynamic memory allocations
    enum class EventKind : std::uint16_t {
        None = 0,

        // Router send pipeline
        Send_Start,
        Send_RouteLookup,
        Send_Shm_Write,
        Send_Socket_Write,
        Send_End,

        // Router recv pipeline
        Recv_Start,
        Recv_Shm_Read,
        Recv_Socket_Read,
        Recv_End,

        // Workers / threads
        SendWorker_Start,
        SendWorker_End,
        ShmRecvWorker_Start,
        ShmRecvWorker_End,
        SocketRecvWorker_Start,
        SocketRecvWorker_End,

        // Broadcasts / discovery
        Broadcast_Send,
        Broadcast_Recv_Start,
        Broadcast_Recv_End,

        Error,
    };

    enum class Severity : std::uint8_t { Debug, Info, Warning, Error, Critical };
    enum class Category : std::uint8_t { General, Router, Shm, Socket, Worker, Broadcast };

    // 16 bytes payload keeps record at 48 bytes (on typical packing).
    static constexpr std::size_t PAYLOAD_BYTES = 16;

    struct EventRecord {
        std::uint64_t time_ns;
        std::uint32_t seq;
        //std::uint32_t thread_id;
        std::uint32_t a;
        std::uint32_t b;
        std::uint32_t c;
        std::uint16_t kind;
        std::uint8_t severity;
        std::uint8_t category;
        std::uint32_t _pad;
        std::uint8_t payload[PAYLOAD_BYTES];
    };
    static_assert(sizeof(EventRecord) == 48, "check packing/size for EventRecord, expected 48 bytes");

    // capacity must be power of two for fast modulo operation
    static constexpr std::size_t EVENT_LOG_CAPACITY = 1024;
    static_assert((EVENT_LOG_CAPACITY & (EVENT_LOG_CAPACITY - 1)) == 0,
                  "EVENT_LOG_CAPACITY must be a power of two");

                  
    
    #ifndef EROIL_ELOG_ENABLED // if cmake has not defined EROIL_ELOG_ENABLED=0, enable logging by default
    #define EROIL_ELOG_ENABLED 1
    #endif
    
    // controls for compile-time logging enable/disable and min severity
    struct EventLogBuildConfig {
        static constexpr bool enabled = (EROIL_ELOG_ENABLED != 0);
        static constexpr Severity min_severity = Severity::Info;
    };

    class EventLog {
        private:
        alignas(64) std::atomic<uint32_t> m_seq{};
        alignas(64) std::atomic<size_t> m_idx{};
        alignas(64) std::array<EventRecord, EVENT_LOG_CAPACITY> m_evt_logs{};

        public:
            // full log with payload copy
            void log(EventKind kind,
                     Severity sev,
                     Category cat,
                     std::uint32_t a=0,
                     std::uint32_t b=0,
                     std::uint32_t c=0,
                     const void* payload=nullptr,
                     std::uint32_t payload_size=0) noexcept;

            // does no memcset/memcpy of payload, for hot paths
            void log_hot(EventKind kind, 
                         Severity sev, 
                         Category cat,
                         std::uint32_t a=0,
                         std::uint32_t b=0,
                         std::uint32_t c=0) noexcept;

            // does no memcset/memcpy of payload, and does not set time_ns, for extremely hot paths
            void log_hot_no_time(EventKind kind, 
                                 Severity sev, 
                                 Category cat,
                                 std::uint32_t a=0,
                                 std::uint32_t b=0,
                                 std::uint32_t c=0) noexcept;
    };

    extern EventLog g_event_log;

    namespace detail {
        // this exists to allow the compiler to remove logging calls at compile time
        // that do not meet the min severity level or if logging is disabled entirely
        template <Severity Sev>
        inline void log(EventKind kind, Category cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            if constexpr (EventLogBuildConfig::enabled && 
               static_cast<std::uint8_t>(Sev) >= static_cast<std::uint8_t>(EventLogBuildConfig::min_severity)) {
                eroil::elog::g_event_log.log(kind, Sev, cat, a, b, c, payload, payload_size);
            }
        }

        template <Severity Sev>
        inline void log_hot(EventKind kind, Category cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            if constexpr (EventLogBuildConfig::enabled && 
               static_cast<std::uint8_t>(Sev) >= static_cast<std::uint8_t>(EventLogBuildConfig::min_severity)) {
                eroil::elog::g_event_log.log_hot(kind, Sev, cat, a, b, c);
            }
        }

        template <Severity Sev>
        inline void log_hot_no_time(EventKind kind, Category cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            if constexpr (EventLogBuildConfig::enabled && 
               static_cast<std::uint8_t>(Sev) >= static_cast<std::uint8_t>(EventLogBuildConfig::min_severity)) {
                eroil::elog::g_event_log.log_hot(kind, Sev, cat, a, b, c);
            }
        }
    }
}

namespace eroil {
    using elog_kind = eroil::elog::EventKind;
    using elog_sev = eroil::elog::Severity;
    using elog_cat = eroil::elog::Category;

    namespace elog {
        // full logs with payload copy
        inline void debug(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            elog::detail::log<elog_sev::Debug>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void info(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            elog::detail::log<elog_sev::Info>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void warn(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            elog::detail::log<elog_sev::Warning>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void error(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            elog::detail::log<elog_sev::Error>(kind, cat, a, b, c, payload, payload_size); 
        }

        inline void crit(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            elog::detail::log<elog_sev::Critical>(kind, cat, a, b, c, payload, payload_size); 
        }

        // logs without payload copy
        inline void debug_fast(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
        elog::detail::log_hot<elog_sev::Debug>(kind, cat, a, b, c); 
        }

        inline void info_fast(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot<elog_sev::Info>(kind, cat, a, b, c); 
        }

        inline void warn_fast(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot<elog_sev::Warning>(kind, cat, a, b, c); 
        }

        inline void error_fast(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot<elog_sev::Error>(kind, cat, a, b, c); 
        }

        inline void crit_fast(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot<elog_sev::Critical>(kind, cat, a, b, c); 
        }

        // logs without payload copy or time stamp
        inline void debug_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot_no_time<elog_sev::Debug>(kind, cat, a, b, c); 
        }

        inline void info_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot_no_time<elog_sev::Info>(kind, cat, a, b, c); 
        }

        inline void warn_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot_no_time<elog_sev::Warning>(kind, cat, a, b, c); 
        }

        inline void error_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot_no_time<elog_sev::Error>(kind, cat, a, b, c); 
        }

        inline void crit_bare(elog_kind kind, elog_cat cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            elog::detail::log_hot_no_time<elog_sev::Critical>(kind, cat, a, b, c); 
        }
    }
}