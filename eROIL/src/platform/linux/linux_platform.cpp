#if defined(EROIL_LINUX)
#include <semaphore.h>
#include <pthread.h>
#include <sched.h>
#include "platform/platform.h"
#include <eROIL/print.h>

namespace eroil::plat {
    static_assert(std::is_same_v<sem_handle, void*>);
    void signal_sem(sem_handle sem, int /*signal_mode*/) {
        if (sem == nullptr) { 
            ERR_PRINT("tried to post() a null semaphore");
            return;
        }

        ::sem_post(static_cast<sem_t*>(sem)); 
    }

    void affinitize_thread(std::thread& t, int cpu) {
        cpu_set_t set;
        ::CPU_ZERO(&set);
        ::CPU_SET(cpu, &set);
        ::pthread_setaffinity_np(t.native_handle(), sizeof(set), &set);
    }

    void affinitize_current_thread(int cpu) {
        cpu_set_t set;
        ::CPU_ZERO(&set);
        ::CPU_SET(cpu, &set);
        ::pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    }
}
#endif