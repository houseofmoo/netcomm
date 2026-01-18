#pragma once
#include <cassert>

#if !defined(NDEBUG) // debug
// if expr is true, terminate    
#define DB_ASSERT(expr, msg) assert((expr) && (msg))

#else
    #define DB_ASSERT(expr, msg) do { (void)0; } while (0)
#endif