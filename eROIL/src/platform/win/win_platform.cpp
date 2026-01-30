#if defined(EROIL_WIN32)
#include "platform/platform.h"
#include "windows_hdr.h"
#include <eROIL/print.h>

namespace eroil::plat {
    static_assert(std::is_same_v<sem_handle, HANDLE>);
    void signal_sem(sem_handle sem) {
        if (sem == nullptr) {
            ERR_PRINT("tried to signal null semaphore");
            return;
        }
        
        if (!::ReleaseSemaphore(static_cast<HANDLE>(sem), 1, nullptr)) {
            ERR_PRINT("sem signal failed, errno=", ::GetLastError());
        }
    }

    void try_signal_sem(sem_handle sem) {
        if (sem == nullptr) {
            return;
        }
        
        if (!::ReleaseSemaphore(static_cast<HANDLE>(sem), 1, nullptr)) {
            ERR_PRINT("sem signal failed, errno=", ::GetLastError());
        }
    }

    void affinitize_thread(std::thread& /*t*/, int /*cpu*/) {
        ERR_PRINT("affinitize_thread() does not work on windows");
        // this does not work, for some reason t.native_handle()
        // is returning a unix-like int-ish handle, not a windows
        // HANDLE

        // single cpu
        //const DWORD_PTR mask = (DWORD_PTR(1) << cpu);
        //SetThreadAffinityMask(t.native_handle(), mask);

        // multiple cpus
        //SetThreadAffinityMask(handle.thread, 1ull << 1 | 1ull << 2);
    }

    void affinitize_current_thread(int cpu) {
        ::SetThreadAffinityMask(::GetCurrentThread(), 1ull << cpu);
    }

    void affinitize_current_thread_to_current_cpu() {
        DWORD cpu = ::GetCurrentProcessorNumber();
        affinitize_current_thread(cpu);
    }
}
#endif