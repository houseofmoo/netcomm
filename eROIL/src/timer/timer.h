#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include "time_log.h"

namespace eroil::time {
    class Timer {
        private:
            std::string m_name;
            uint64_t m_start_ns;

        public:
            Timer(std::string name);
            ~Timer();
    };
}
