#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include "time_log.h"

namespace eroil::time {
    class Timer {
        private:
            std::string m_name;
            std::chrono::steady_clock::time_point m_start;

        public:
            Timer(std::string name);
            ~Timer();
    };
}
