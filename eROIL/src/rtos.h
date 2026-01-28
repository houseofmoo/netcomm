#pragma once
#include <chrono>
#include "types/const_types.h"
#include "types/handles.h"

namespace eroil {
    // inline void RTOS_Raw_Time_To_Usec(int /*clock_type*/, eroil::RTOSTime timestamp, double* out) {
    //     constexpr uint32_t USEC_PER_SEC = 1000000;
    //     constexpr uint32_t NSEC_PER_USEC = 1000;
    //     *out = static_cast<double>(timestamp.uiMsb * USEC_PER_SEC) + 
    //         static_cast<double>(timestamp.uiLsb / NSEC_PER_USEC);
    // }

    inline iosb::RTOSTime RTOS_Current_Time_Raw() {
        // original was linux only, rewrote for portability
        // struct timespec ts;
        // int status = clock_gettime(CLOCK_REALTIME, &ts);
        // time->uiMsb = (status == 0) ? static_cast<unsigned int>(ts.tv_sec) : 0;
        // time->uiLsb = (status == 0) ? static_cast<unsigned int>(ts.tv_nsec) : 0;
        iosb::RTOSTime out;

        const auto now = std::chrono::system_clock::now();
        const auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now).time_since_epoch();
        
        const auto sec = std::chrono::duration_cast<std::chrono::seconds>(ns);
        const auto nsec = ns - sec;

        out.uiMsb = static_cast<unsigned int>(sec.count());
        out.uiLsb = static_cast<unsigned int>(nsec.count());
        return out;
    }
}