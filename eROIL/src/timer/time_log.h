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
}