#include "evtlog.h"
#include <cstring>
#include <thread>
#include <chrono>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // for __rdtsc()
    #if defined(_MSC_VER)
        #include <intrin.h>
    #elif defined(__GNUC__) || defined(__clang__)
        #include <x86intrin.h>
    #endif
#endif

// notify build system of evtlog status, just nice to confirm
#if EROIL_ELOG_ENABLED
#pragma message("EROIL ELOG ENABLED")
#else
#pragma message("EROIL ELOG DISABLED")
#endif

namespace eroil::evtlog {
    EventLog g_event_log;
    static uint64_t estimated_tsc_hz = 0;

    // get current time in nanoseconds using steady clock
    // inline uint64_t now_ns_steady() noexcept {
    //     return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
    //         std::chrono::steady_clock::now().time_since_epoch()
    //     ).count();
    // }

    // get id of current thread as a hashed uint32_t
    // static std::uint32_t hash_tid() noexcept {
    //     auto id = std::this_thread::get_id();
    //     return (std::uint32_t)std::hash<std::thread::id>{}(id);
    // }

    inline uint64_t measure_tsc_hz() {
        // get ticks per second
        using clock = std::chrono::steady_clock;

        auto t0 = clock::now();
        uint64_t c0 = __rdtsc();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto t1 = clock::now();
        uint64_t c1 = __rdtsc();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        return (c1 - c0) * 1'000'000'000ull / ns;
    }

    uint64_t estimate_tsc_hz() noexcept {
        estimated_tsc_hz = measure_tsc_hz();
        return estimated_tsc_hz;
    }

    inline uint64_t now_fast() noexcept {
        // returns a fast increasing counter, representing a cpu "tick" moment
        // compare two calls to get an idea of elapsed time in cpu ticks
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        return __rdtsc();
        #else
        return 0;//now_ns_steady();
        #endif
    }



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

            // mark invalid while writing
            r.commit_seq.store(0, std::memory_order_relaxed);

            // write data
            r.tick = now_fast();
            //r.thread_id = hash_tid();
            r.a = a;
            r.b = b;
            r.c = c;
            r.kind = static_cast<std::uint16_t>(kind);
            r.severity = static_cast<std::uint8_t>(sev);
            r.category = static_cast<std::uint8_t>(cat);

            if (payload && payload_size) {
                const std::uint32_t n = (payload_size < PAYLOAD_BYTES) ? payload_size : PAYLOAD_BYTES;
                std::memcpy(r.payload, payload, n);
                if (payload_size < PAYLOAD_BYTES) {
                    std::memset(r.payload + n, 0, PAYLOAD_BYTES - n);
                }
            } else {
                std::memset(r.payload, 0, PAYLOAD_BYTES);
            }

            r.commit_seq.store(seq, std::memory_order_release);
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

            // mark invalid while writing
            r.commit_seq.store(0, std::memory_order_relaxed);

            r.tick = now_fast();
            //r.thread_id = hash_tid();
            r.a = a;
            r.b = b;
            r.c = c;
            r.kind = static_cast<std::uint16_t>(kind);
            r.severity = static_cast<std::uint8_t>(sev);
            r.category = static_cast<std::uint8_t>(cat);
            r.commit_seq.store(seq, std::memory_order_release);
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
           
            // mark invalid while writing
            r.commit_seq.store(0, std::memory_order_relaxed);

            r.tick = 0;
            //r.thread_id = hash_tid();
            r.a = a;
            r.b = b;
            r.c = c;
            r.kind = static_cast<std::uint16_t>(kind);
            r.severity = static_cast<std::uint8_t>(sev);
            r.category = static_cast<std::uint8_t>(cat);
            r.commit_seq.store(seq, std::memory_order_release);
        }

        void EventLog::write_evtlog() noexcept {

            // loop through logs and write them...
            // check the seq is what we expect
            //uint32_t seq = r.commit_seq.load(std::memory_order_acquire);
            //if (seq == 0) continue;

            // example usage:
            // do this when writing the output? 
            // probably measure once during startup and store the value
            // uint64_t delta_cycles = start.ticks - end.ticks;
            // uint64_t delta_ns = (delta_cycles * 1'000'000'000ull) / estimated_tsc_hz;

            // binary write: write estimated_tsc_hz at start of file
            // then dump entire event log as binary

            // text writer: write estimated_tsc_hz at start of file
            // then write each event log line by line with human readable format

            // can do the math on their own
        }
}