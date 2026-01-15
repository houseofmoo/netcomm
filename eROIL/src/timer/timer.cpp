#include "timer.h"
#include "time_log.h"
#include <chrono>

namespace eroil::time {
    Timer::Timer(std::string name) : m_name(std::move(name)), m_start_ns(0) {
        auto start_now = std::chrono::steady_clock::now();
        m_start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(start_now.time_since_epoch()).count(); 
    }

    Timer::~Timer() {
        auto end_now = std::chrono::steady_clock::now();
        auto end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_now.time_since_epoch()).count(); 
        time_log.insert(std::move(m_name), m_start_ns, end_ns);
    }
}