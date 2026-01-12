#pragma once

#include <iostream>
#include <mutex>
#include <sstream>

namespace print {
    inline std::mutex print_mtx;
    inline int my_id = 0;

    template <typename... Args>
    inline void write(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(print_mtx);
        (std::cout << "[ " << my_id << " ] " << ... << args) << std::endl;
    }

    template <typename... Args>
    inline void error(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(print_mtx);
        (std::cerr << "[ " << my_id << " ] " << "ERROR: " << ... << args) << std::endl;
    }

    template <typename... Args>
    inline void log(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(print_mtx);
        (std::clog << "[ " << my_id << " ] " << ... << args) << std::endl;
    }
}

#ifndef NDEBUG // debug
    #define PRINT_ID(...) print::write(this->LogId(), __VA_ARGS__);
    #define ERR_PRINT_ID(...) print::error(this->LogId(), __VA_ARGS__);
    #define LOG_ID(...) print::log(this->LogId(), __VA_ARGS__);

    #define PRINT(...) print::write(__VA_ARGS__);
    #define ERR_PRINT(...) print::error(__VA_ARGS__);
    #define LOG(...) print::log(__VA_ARGS__);
#else // release
    #define PRINT_ID(...)
    #define ERR_PRINT_ID(...) print::error(this->LogId(), __VA_ARGS__);
    #define LOG_ID(...)

    #define PRINT(...)
    #define ERR_PRINT(...) print::error(__VA_ARGS__);
    #define LOG(...) print::log(__VA_ARGS__);
#endif