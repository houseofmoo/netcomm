#pragma once

#include <iostream>
#include <utility>
#include <mutex>

namespace print {
    namespace detail {
        inline int __id;
        inline std::mutex __mtx;
    }

    inline void set_id(int id) { detail::__id = id; }

    template <typename... Args>
    inline void write(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(detail::__mtx);
        std::cout << "[ " << detail::__id << " ] ";
        ((std::cout << std::forward<Args>(args)), ...);
        std::cout << std::endl;
    }

    template <typename... Args>
    inline void ERR_PRINT(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(detail::__mtx);
        std::cerr << "[ " << detail::__id << " ] ERROR: ";
        ((std::cerr << std::forward<Args>(args)), ...);
        std::cerr << std::endl;
    }

    template <typename... Args>
    inline void log(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(detail::__mtx);
        std::clog << "[ " << detail::__id << " ] ";
        ((std::clog << std::forward<Args>(args)), ...);
        std::clog << std::endl;
    }
}

#ifndef NDEBUG // debug
    #define PRINT(...) print::write(__VA_ARGS__)
    #define ERR_PRINT(...) print::ERR_PRINT(__VA_ARGS__)
    #define LOG(...) print::log(__VA_ARGS__)
#else // release
    #define PRINT(...) do { (void)0; } while (0)
    #define ERR_PRINT(...) print::ERR_PRINT(__VA_ARGS__)
    #define LOG(...) print::log(__VA_ARGS__)
#endif