#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "evtrecord.h"

namespace eroil::evtlog {
    // this whole logging system is designed to be as fast as possible with minimal overhead
    // so we use a fixed size ring buffer and avoid any dynamic memory allocations

    // capacity must be power of two for fast modulo operation
    static constexpr std::size_t EVENT_LOG_CAPACITY = 1024;
    static_assert((EVENT_LOG_CAPACITY & (EVENT_LOG_CAPACITY - 1)) == 0,
                  "EVENT_LOG_CAPACITY must be a power of two");
                  
    
    // controls for compile-time logging enable/disable and min severity
    struct EventLogBuildConfig {
        #if EROIL_ELOG_ENABLED
            static constexpr bool enabled = true;
        #else
            static constexpr bool enabled = false;
        #endif
        static constexpr Severity min_severity = Severity::Warning;
    };

    class EventLog {
        private:
            alignas(64) std::atomic<uint32_t> m_seq{};
            alignas(64) std::atomic<std::size_t> m_idx{};
            alignas(64) std::array<EventRecord, EVENT_LOG_CAPACITY> m_evt_logs{};

        public:
            // full log with payload copy
            void log_payload(EventKind kind,
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

            // does no memcset/memcpy of payload, and does not set tick, for extremely hot paths
            void log_hot_no_time(EventKind kind, 
                                 Severity sev, 
                                 Category cat,
                                 std::uint32_t a=0,
                                 std::uint32_t b=0,
                                 std::uint32_t c=0) noexcept;

            void write_evtlog() noexcept;
    };

    // do not access this directly, use evtlog_api.h wrappers
    extern EventLog g_event_log;

    namespace detail {
        //
        // TEMPLATE WRAPPERS FOR LOGGING
        // DO NOT CALL THESE DIRECTLY
        //
        // these exist to allow the compiler to remove logging calls at compile time
        // that do not meet the min severity level or if logging is disabled entirely
        // set event logging enabled via compiler flags (DEROIL_ELOG_ENABLED=1)
        // change min servity via EventLogBuildConfig
        //

        // black magic to make our constexpr evaluation work on linux
        template <auto>
        inline constexpr bool dependent_false_v = false;

        template <Severity Sev>
        inline void log_payload(EventKind kind, Category cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0, const void* payload=nullptr, std::uint32_t payload_size=0) noexcept {
            if constexpr (EventLogBuildConfig::enabled && 
                          Sev >= EventLogBuildConfig::min_severity) {
                // double check this code is removed if EventLogBuildConfig::enabled = false
                static_assert(EventLogBuildConfig::enabled || dependent_false_v<Sev>, "should never compile when disabled");
                eroil::evtlog::g_event_log.log_payload(kind, Sev, cat, a, b, c, payload, payload_size);
            }
        }

        template <Severity Sev>
        inline void log_hot(EventKind kind, Category cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            if constexpr (EventLogBuildConfig::enabled && 
                          Sev >= EventLogBuildConfig::min_severity) {
                // double check this code is removed if EventLogBuildConfig::enabled = false
                static_assert(EventLogBuildConfig::enabled || dependent_false_v<Sev>, "should never compile when disabled");
                eroil::evtlog::g_event_log.log_hot(kind, Sev, cat, a, b, c);
            }
        }

        template <Severity Sev>
        inline void log_hot_no_time(EventKind kind, Category cat, std::uint32_t a=0, std::uint32_t b=0, std::uint32_t c=0) noexcept {
            if constexpr (EventLogBuildConfig::enabled && 
                          Sev >= EventLogBuildConfig::min_severity) {
                // double check this code is removed if EventLogBuildConfig::enabled = false
                static_assert(EventLogBuildConfig::enabled || dependent_false_v<Sev>, "should never compile when disabled");
                eroil::evtlog::g_event_log.log_hot_no_time(kind, Sev, cat, a, b, c);
            }
        }
    }
}