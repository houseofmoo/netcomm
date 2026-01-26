#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include "types/types.h"

namespace eroil::time {

    struct TimeInfo {
        uint64_t duration_us;
    };

    class Timelog {
        private:
            std::mutex m_mtx;
            std::unordered_map<std::string, std::vector<TimeInfo>> m_log;

        public:
            Timelog();
            ~Timelog() = default;
            void insert(std::string name, uint64_t duration_us);
            void timed_run(const NodeId id, uint64_t collection_time);
            void write_log(const std::string& filename);
    };

    extern Timelog time_log;

    class TimeLogTimer {
        private:
            std::string m_name;
            std::chrono::steady_clock::time_point m_start;

        public:
            TimeLogTimer(std::string name) : m_name(std::move(name)), m_start{} {
                m_start = std::chrono::steady_clock::now();
            }

            ~TimeLogTimer() {
                auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - m_start
                ).count();

                time_log.insert(std::move(m_name), duration_us );
            }
    };
}