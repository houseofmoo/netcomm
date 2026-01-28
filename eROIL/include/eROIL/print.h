#pragma once

#include <iostream>
#include <utility>
#include <mutex>
#include <cstdint>

#if defined(__clang__) || defined(__GNUC__)
    #define FUNC_SIG __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define FUNC_SIG __FUNCSIG__
#else
    #define FUNC_SIG __func__
#endif

namespace print {
    namespace detail {
        inline int32_t __id;
        inline std::mutex __mtx;
    }

    inline void set_id(int32_t id) { detail::__id = id; }

    template <typename... Args>
    inline void db_print(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(detail::__mtx);
        std::cout << "[ " << detail::__id << " ] ";
        ((std::cout << std::forward<Args>(args)), ...);
        std::cout << std::endl;
    }

    template <typename... Args>
    inline void error_print(const char* func, Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(detail::__mtx);
        std::cerr << "[ " << detail::__id << " ] ERROR ";
        std::cerr << func << ": ";
        ((std::cerr << std::forward<Args>(args)), ...);
        std::cerr << std::endl;
    }

    template <typename... Args>
    inline void info_print(Args&&... args) noexcept {
        std::lock_guard<std::mutex> lock(detail::__mtx);
        std::clog << "[ " << detail::__id << " ] INFO: ";
        ((std::clog << std::forward<Args>(args)), ...);
        std::clog << std::endl;
    }
}

#if !defined(NDEBUG) // debug
    #define PRINT(...) print::db_print(__VA_ARGS__)
    #define ERR_PRINT(...) print::error_print(FUNC_SIG, __VA_ARGS__)
    #define LOG(...) print::info_print(__VA_ARGS__)
#else // release
    #define PRINT(...) do { (void)0; } while (0)
    #define ERR_PRINT(...) print::error_print(FUNC_SIG, __VA_ARGS__)
    #define LOG(...) print::info_print(__VA_ARGS__)
#endif