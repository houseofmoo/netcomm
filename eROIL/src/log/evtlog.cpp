#include "evtlog.h"
#include <cstring>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <eROIL/print.h>
#include "platform/platform.h"

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
    
    
    inline uint64_t now_fast() noexcept {
        // returns a fast increasing counter, representing a cpu "tick" moment
        // compare two calls to get an idea of elapsed time in cpu ticks
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        return __rdtsc();
        #else
        return 0; //now_ns_steady(); as an option for non x86 targets
        #endif
    }

    inline uint64_t measure_tsc_hz() noexcept {
        // get ticks per second
        uint64_t c0 = now_fast();
        if (c0 == 0) return 0; // this failed on the target platform
        
        auto t0 = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto t1 = std::chrono::steady_clock::now();

        uint64_t c1 = now_fast(); 
        if (c1 == 0) return 0; // this failed on the target platform

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        if (ns <= 0) return 0;

        return static_cast<uint64_t>((c1 - c0) * 1'000'000'000ull / static_cast<uint64_t>(ns));
    }

    uint64_t estimate_tsc_hz() noexcept {
        estimated_tsc_hz = measure_tsc_hz();
        return estimated_tsc_hz;
    }

    void EventLog::log_payload(EventKind kind,
                               Severity sev,
                               Category cat,
                               std::int32_t a,
                               std::int32_t b,
                               std::int32_t c,
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
            r.flags = HAS_TICK;

            if (payload != nullptr && payload_size > 0) {
                r.flags |= HAS_PAYLOAD;
                const std::uint32_t size = (payload_size < PAYLOAD_BYTES) ? payload_size : PAYLOAD_BYTES;
                r.payload_len = static_cast<uint16_t>(size);
                std::memcpy(r.payload, payload, size);
                
                // zero remining bytes
                if (payload_size < PAYLOAD_BYTES) {
                    std::memset(r.payload + size, 0, PAYLOAD_BYTES - size);
                }
            } else {
                r.payload_len = 0;
                std::memset(r.payload, 0, PAYLOAD_BYTES);
            }

            r.commit_seq.store(seq, std::memory_order_release);
        }

        void EventLog::log_hot(EventKind kind, 
                               Severity sev, 
                               Category cat,
                               std::int32_t a,
                               std::int32_t b,
                               std::int32_t c) noexcept {
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
            r.flags = HAS_TICK;
            r.payload_len = 0;
            r.commit_seq.store(seq, std::memory_order_release);
        }

        void EventLog::log_hot_no_time(EventKind kind, 
                                       Severity sev, 
                                       Category cat,
                                       std::int32_t a,
                                       std::int32_t b,
                                       std::int32_t c) noexcept {
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
            r.flags = 0;
            r.payload_len = 0;
            r.commit_seq.store(seq, std::memory_order_release);
        }

        void EventLog::write_evtlog() noexcept {
            for (const auto& rec : g_event_log.m_evt_logs) {
                // confirm this value is not currently being written to
                const uint32_t seq = rec.commit_seq.load(std::memory_order_acquire);
                if (seq == 0) { continue; }

                // snap shot the record
                EventRecordSnapshot snapshot;
                std::memcpy(&snapshot, &rec, offsetof(EventRecord, commit_seq));

                // was record changed while we were copying?
                if (seq != rec.commit_seq.load(std::memory_order_relaxed)) { continue; }

                std::ostringstream oss;
                oss << snapshot.tick << ","
                    << snapshot.a << ","
                    << snapshot.b << ","
                    << snapshot.c << ","
                    << snapshot.kind << ","
                    << snapshot.severity << ","
                    << snapshot.category << ","
                    << snapshot.flags << ","
                    << snapshot.payload_len << ",";

                const std::uint32_t size = (snapshot.payload_len <= PAYLOAD_BYTES) ? snapshot.payload_len : PAYLOAD_BYTES;
                for (size_t i = 0; i < PAYLOAD_BYTES; ++i) {
                    const uint8_t val = (i < static_cast<size_t>(size)) ? snapshot.payload[i] : 0;
                    oss << +val << ",";
                }
       
                oss << seq << "\n";
                LOG(oss.str());
            }
        }

        void EventLog::write_evtlog(const std::string& directory) noexcept {
            std::error_code ec;
            std::filesystem::path dir(directory);
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                ERR_PRINT("failed to create dir=", dir.string(), " ec=", ec.message());
                return;
            }
            
            const std::string filename = "evtlog_" + plat::timestamp_str() + ".log";
            const std::filesystem::path filepath = dir / filename;

            std::ofstream file(filepath, std::ios::out);
            if (!file.is_open()) {
                ERR_PRINT("filed to open file=", filepath.string());
                return;
            }

            for (const auto& rec : g_event_log.m_evt_logs) {
                // confirm this value is not currently being written to
                const uint32_t seq = rec.commit_seq.load(std::memory_order_acquire);
                if (seq == 0) { continue; }

                // snap shot the record
                EventRecordSnapshot snapshot;
                std::memcpy(&snapshot, &rec, offsetof(EventRecord, commit_seq));

                // was record changed while we were copying?
                if (seq != rec.commit_seq.load(std::memory_order_relaxed)) { continue; }

                file << snapshot.tick << ","
                     << snapshot.a << ","
                     << snapshot.b << ","
                     << snapshot.c << ","
                     << snapshot.kind << ","
                     << snapshot.severity << ","
                     << snapshot.category << ","
                     << snapshot.flags << ","
                     << snapshot.payload_len << ",";

                const std::uint32_t size = (snapshot.payload_len <= PAYLOAD_BYTES) ? snapshot.payload_len : PAYLOAD_BYTES;
                for (size_t i = 0; i < PAYLOAD_BYTES; ++i) {
                    const uint8_t val = (i < static_cast<size_t>(size)) ? snapshot.payload[i] : 0;
                    file << +val << ",";
                }

                file << seq << "\n";
            }
        }
}