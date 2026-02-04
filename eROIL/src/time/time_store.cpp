#include "time_store.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <string>
#include <eROIL/print.h>

namespace eroil::time {
    TimeStore time_store;

    TimeStore::TimeStore() : m_store({}) {
        // reserve 30 slots a head of time to prevent rehashes unless we go over
        m_store.reserve(30); 
    }

    void TimeStore::insert(std::string name, uint64_t duration_us) {
        std::lock_guard lock(m_mtx);
        auto& vec = m_store.try_emplace(std::move(name)).first->second;
        vec.push_back(Sample{ duration_us });
    }

    void TimeStore::clear() {
        std::lock_guard lock(m_mtx);
        m_store.clear();
    }
    
    void TimeStore::timed_run(const NodeId id, const uint64_t duration_ms) {
        // waits for collection_time_ms milliseconds then writes log to file
        std::thread([this, id, duration_ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
            std::string filename = "timelog_node_" + std::to_string(id) + ".txt";
            write_log(filename);
        }).detach();
    }

    void TimeStore::write_log(const std::string& filename) {
        std::lock_guard lock(m_mtx);
        std::string dir = "timelog";
        
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directory(dir);
        }
        
        std::filesystem::path filepath = dir + "/" + filename;
        std::ofstream file(filepath, std::ios::out);
        if (!file.is_open()) {
            ERR_PRINT("failed to open timelog file for writing: ", filepath);
            return;
        }

        for (const auto& [name, times] : m_store) {
            file << name << " timings (us):\n";
            for (const auto& duration : times) {
                file << duration.us << "\n";
            }
            file << "\n";
        }
    }
}