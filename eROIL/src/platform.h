#pragma once
#include "types/types.h"
#include <eROIL/print.h>

// PLATFORM SPECIFIC THINGS LIVE HERE

#if defined(EROIL_LINUX)
    #include <semaphore.h>
#elif defined(EROIL_WIN32)
    #include "windows_hdr.h"
#else
    #error "Must define EROIL_LINUX or EROIL_WIN32"
#endif

namespace eroil {
    #if defined(EROIL_LINUX)
        static_assert(std::is_same_v<sem_handle, sem_t*>);
    #elif defined(EROIL_WIN32)
        static_assert(std::is_same_v<sem_handle, HANDLE>);
    #endif

    inline void signal_recv_sem(sem_handle sem, int /*signal_mode*/) {
        #if defined(EROIL_LINUX)
        if (sem) { sem_post(sem); }
        #elif defined(EROIL_WIN32)
        if (sem && !::ReleaseSemaphore(sem, 1, nullptr)) {
            ERR_PRINT("sem signal failed, errno=",::GetLastError());
        }
        #endif
    }
}