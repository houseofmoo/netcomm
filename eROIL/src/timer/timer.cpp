#include "timer.h"
#include "time_log.h"

namespace eroil::time {
    Timer::Timer(std::string name) : m_name(std::move(name)), m_start{} {
        m_start = std::chrono::steady_clock::now();
    }

    Timer::~Timer() {
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_start
        ).count();

        time_log.insert(std::move(m_name), duration_us );
    }
}