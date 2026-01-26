#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include "time_log.h"

namespace eroil::time {
    class Timer {
        private:
            std::chrono::steady_clock::time_point m_start;
            bool m_running;

        public:
            Timer() : m_start{}, m_running{false} {}
            ~Timer() = default;

            void start() {
                if (!m_running) {
                    m_running = true;
                    m_start = std::chrono::steady_clock::now();
                }
            }

            void stop() {
                m_running = false;
            }

            int64_t duration() {
                auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_start
                ).count();
                return static_cast<int64_t>(dur);
            }
    };
}
