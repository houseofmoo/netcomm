#include "evtlog.h"
// #include <thread>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // for __rdtsc()
    #if defined(_MSC_VER)
        #include <intrin.h>
    #elif defined(__GNUC__) || defined(__clang__)
        #include <x86intrin.h>
    #endif
#endif

// notify build system of evtlog status, just nice to confirm
#if defined(EROIL_ELOG_ENABLED) && EROIL_ELOG_ENABLED == 1
#pragma message("EROIL ELOG ENABLED")
#else
#pragma message("EROIL ELOG DISABLED")
#endif

namespace eroil::evtlog {
    EventLog g_event_log;

    // inline uint64_t now_ns_steady() noexcept {
    //     return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
    //         std::chrono::steady_clock::now().time_since_epoch()
    //     ).count();
    // }

    inline uint64_t now_fast() noexcept {
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        return __rdtsc();
        #else
        return 0;//now_ns_steady();
        #endif
    }

    // static std::uint32_t hash_tid() noexcept {
    //     auto id = std::this_thread::get_id();
    //     return (std::uint32_t)std::hash<std::thread::id>{}(id);
    // }

    void EventLog::log_payload(EventKind kind,
                               Severity sev,
                               Category cat,
                               std::uint32_t a,
                               std::uint32_t b,
                               std::uint32_t c,
                               const void* payload,
                               std::uint32_t payload_size) noexcept {

            const std::uint32_t seq = m_seq.fetch_add(1, std::memory_order_relaxed);
            const std::size_t idx = m_idx.fetch_add(1, std::memory_order_relaxed) & (EVENT_LOG_CAPACITY - 1);

            EventRecord& r = m_evt_logs[idx];

            r.tick = now_fast();
            r.seq = seq;
            //r.thread_id = hash_tid();
            r.a = a;
            r.b = b;
            r.c = c;
            r.kind = (std::uint16_t)kind;
            r.severity = (std::uint8_t)sev;
            r.category = (std::uint8_t)cat;

            if (payload && payload_size) {
                const std::uint32_t n = (payload_size < PAYLOAD_BYTES) ? payload_size : PAYLOAD_BYTES;
                std::memcpy(r.payload, payload, n);
                if (payload_size < PAYLOAD_BYTES) {
                    std::memset(r.payload + n, 0, PAYLOAD_BYTES - n);
                }
            } else {
                std::memset(r.payload, 0, PAYLOAD_BYTES);
            }
        }

        void EventLog::log_hot(EventKind kind, 
                               Severity sev, 
                               Category cat,
                               std::uint32_t a,
                               std::uint32_t b,
                               std::uint32_t c) noexcept {
            const std::uint32_t seq = m_seq.fetch_add(1, std::memory_order_relaxed);
            const std::size_t idx = m_idx.fetch_add(1, std::memory_order_relaxed) & (EVENT_LOG_CAPACITY - 1);

            EventRecord& r = m_evt_logs[idx];
            r.tick = now_fast();
            r.seq = seq;
            //r.thread_id = hash_tid();
            r.a = a;
            r.b = b;
            r.c = c;
            r.kind = (std::uint16_t)kind;
            r.severity = (std::uint8_t)sev;
            r.category = (std::uint8_t)cat;
        }

        void EventLog::log_hot_no_time(EventKind kind, 
                                       Severity sev, 
                                       Category cat,
                                       std::uint32_t a,
                                       std::uint32_t b,
                                       std::uint32_t c) noexcept {
            const std::uint32_t seq = m_seq.fetch_add(1, std::memory_order_relaxed);
            const std::size_t idx = m_idx.fetch_add(1, std::memory_order_relaxed) & (EVENT_LOG_CAPACITY - 1);

            EventRecord& r = m_evt_logs[idx];
            r.tick = 0;
            r.seq = seq;
            //r.thread_id = hash_tid();
            r.a = a;
            r.b = b;
            r.c = c;
            r.kind = (std::uint16_t)kind;
            r.severity = (std::uint8_t)sev;
            r.category = (std::uint8_t)cat;
        }
}