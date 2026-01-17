#pragma once
#include <thread>
#include "types/types.h"

namespace eroil::plat {
    void signal_sem(sem_handle sem, int signal_mode);
    void signal_sem(sem_handle sem);
    void try_signal_sem(sem_handle sem, int signal_mode);
    void try_signal_sem(sem_handle sem);

    void affinitize_thread(std::thread& t, int cpu);
    void affinitize_current_thread(int cpu);
    void affinitize_current_thread_to_current_cpu();
}