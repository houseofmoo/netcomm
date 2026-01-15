#include "time_log.h"

namespace eroil::time {
    Timelog time_log;

    Timelog::Timelog() : m_log({}) {
        // reserve 30 slots a head of time to prevent rehashes unless we go over
        m_log.reserve(30); 
    }

    void Timelog::insert(std::string name, uint64_t time_ns) {
        // creates index if it doesnt exist
        m_log[name].emplace_back(TimeInfo{ time_ns});
    }
}