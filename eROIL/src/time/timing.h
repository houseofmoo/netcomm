#pragma once
#include "types/const_types.h"

#if !defined(NDEBUG) // debug
    #include "time_store.h"
#endif

namespace eroil {

    #if !defined(NDEBUG) // debug
        #define DB_EROIL_CONCAT2(a, b) a##b
        #define DB_EROIL_CONCAT(a, b) DB_EROIL_CONCAT2(a, b)

        #define DB_SCOPED_TIMER(name) \
                time::ScopedTimer DB_EROIL_CONCAT(_etimer_, __COUNTER__)(time::time_store, name)
    #else
        #define DB_SCOPED_TIMER(name) ((void)0)
    #endif

    namespace time {
        #if !defined(NDEBUG) // debug
            inline void clear() {
                time_store.clear();
            }

            inline void timed_run(NodeId id, uint64_t duration_ms) {
                time_store.timed_run(id, duration_ms);
            }

            inline void write_log(const std::string& filename) {
                time_store.write_log(filename);
            }
        #else // release
            inline void clear() {}

            inline void timed_run(NodeId id, uint64_t duration_ms) {
                (void)id; 
                (void)duration_ms;
            }

            inline void write_log(const std::string& filename) {
                (void)filename;
            }

        #endif
    }

}
