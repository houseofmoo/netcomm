#include "time_log.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <string>
#include <eROIL/print.h>

namespace eroil::time {
    Timelog time_log;

    Timelog::Timelog() : m_log({}) {
        // reserve 30 slots a head of time to prevent rehashes unless we go over
        m_log.reserve(30); 
    }

    void Timelog::insert(std::string name, uint64_t duration_us) {
        std::lock_guard lock(m_mtx);
        // creates index if it doesnt exist
        m_log[name].emplace_back(TimeInfo{ duration_us});
    }
    
    void Timelog::timed_run(const NodeId id, const uint64_t collection_time_ms) {
        // waits for collection_time_ms milliseconds then writes log to file
        std::thread t([this, id, collection_time_ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(collection_time_ms));
            std::string filename = "timelog_node_" + std::to_string(id) + ".txt";
            write_log(filename);
        });
        t.detach();
    }

    void Timelog::write_log(const std::string& filename) {
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

        for (const auto& [name, times] : m_log) {
            file << name << " timings (us):\n";
            for (const auto& time_info : times) {
                file << time_info.duration_us << "\n";
            }
            file << "\n";
        }

        file.close();
    }
}