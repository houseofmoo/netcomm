#pragma once
#include <cassert>
#include <cstdint>
#include <string_view>

#if !defined(NDEBUG) // debug
// if expr is true, terminate    
#define DB_ASSERT(expr, msg) assert((expr) && (msg))

#else
    #define DB_ASSERT(expr, msg) do { (void)0; } while (0)
#endif