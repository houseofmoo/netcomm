#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <chrono>
#include "types/const_types.h"

namespace eroil::time {
    // NOTE: for most use cases, use the helper macros in timing.h to collect timing information
    // DB_SCOPED_TIMER(name) -> time from macro declaration to going out of the current scope
    // DB_TIME_RUN(id, duration_ms) -> collect time data until duration expires, then dump the collected data to a file
    //
    // DO NOT DO TIMING COLLECTIONS IN RELEASE CONFIG! If you use the macros, they will become no-ops in
    // release config
    struct Sample {
        uint64_t us; // microseconds
    };

    class TimeStore {
        private:
            std::mutex m_mtx;
            std::unordered_map<std::string, std::vector<Sample>> m_store;

        public:
            TimeStore();
            ~TimeStore() = default;
            void insert(std::string name, uint64_t duration_us);
            void clear();
            void timed_run(const NodeId id, uint64_t duration_ms);
            void write_log(const std::string& filename);
    };

    extern TimeStore time_store;

    class ScopedTimer {
        private:
            TimeStore* m_store;
            std::string m_name;
            std::chrono::steady_clock::time_point m_start;

        public:
            ScopedTimer(TimeStore& store, std::string name) : 
                m_store(&store), m_name(std::move(name)), m_start{std::chrono::steady_clock::now()} {}

            ~ScopedTimer() {
                auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - m_start
                ).count();

                m_store->insert(std::move(m_name), static_cast<uint64_t>(duration_us) );
            }
    };
}