#pragma once
#include <queue>
#include <memory>
#include <chrono>



namespace eroil::log {
    using LogTime = std::chrono::time_point<std::chrono::system_clock>;

    struct ILogEntry {
        LogTime time;
        virtual void write() = 0;
    };



    class EventLog {
        std::queue<std::unique_ptr<ILogEntry>> m_log_q;
    };

    // event log
    // a queue (may 1000) of the events that occured
    // and we can dump when required
    // events are structs with data that implement a "write" function so they can
    // be dumped to a file (or terminal?)
}