#pragma once
#include <thread>
#include "types/const_types.h"

namespace eroil::plat {
    void signal_sem(sem_handle sem);
    void try_signal_sem(sem_handle sem);

    void affinitize_thread(std::thread& t, uint32_t cpu);
    void affinitize_current_thread(uint32_t cpu);
    void affinitize_current_thread_to_current_cpu();
}