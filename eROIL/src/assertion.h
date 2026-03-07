#pragma once
#include <cassert>

namespace eroil {
    #if !defined(NDEBUG)
        #define DB_ASSERT(expr, msg) assert((expr) && (msg))
    #else
        #define DB_ASSERT(expr, msg) do { (void)0; } while (0)
    #endif
}