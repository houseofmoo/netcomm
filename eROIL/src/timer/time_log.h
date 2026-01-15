#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace eroil::time {
    struct TimeInfo {
        uint64_t start_time;
        uint64_t end_time;
    };

    class Timelog {
        private:
            std::unordered_map<std::string, std::vector<TimeInfo>> m_log;

        public:
            Timelog();
            ~Timelog() = default;
            void insert(std::string name, uint64_t start, uint64_t end);
    };

    extern Timelog time_log;
}