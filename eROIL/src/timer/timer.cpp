#include "timer.h"
#include "time_log.h"

namespace eroil::time {
    Timer::Timer(std::string name) : m_name(std::move(name)), m_start{} {
        m_start = std::chrono::steady_clock::now();
    }

    Timer::~Timer() {
        auto end = std::chrono::steady_clock::now().time_since_epoch() - m_start.time_since_epoch();
        time_log.insert(std::move(m_name), end.count() );
    }
}