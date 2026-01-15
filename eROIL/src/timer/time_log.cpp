#include "time_log.h"

namespace eroil::time {
    Timelog time_log;

    Timelog::Timelog() : m_log({}) {}

    void Timelog::insert(std::string name, uint64_t start, uint64_t end) {
        // creates index if it doesnt exist
        m_log[name].push_back(TimeInfo{ start, end });
    }
}