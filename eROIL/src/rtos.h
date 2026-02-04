#pragma once
#include <chrono>
#include "types/const_types.h"
#include "types/handles.h"

namespace eroil::rtos {
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

    inline uint32_t get_roil_id(NodeId id) {
        if (id < 0 || id > 19) return 0;

        // this is a dedicated list of ID's provided by roil
        struct NAE_Node {
            int32_t iNodeId;
            int32_t iModuleId;
            int32_t iCoreId;
            uint32_t iRoilNodeId; 
        };

        static const NAE_Node nodes[20] = {
                        // iNodeId, iModuleId, iCoreId, iRoilNodeId
            /* sp00 */ {   0,       0,         0,       0x73703030 },
            /* sp01 */ {   1,       0,         1,       0x73703031 },
            /* sp02 */ {   2,       0,         2,       0x73703032 },
            /* sp03 */ {   3,       0,         3,       0x73703033 },
            /* sp04 */ {   4,       1,         0,       0x73703034 },
            /* sp05 */ {   5,       1,         1,       0x73703035 },
            /* sp06 */ {   6,       1,         2,       0x73703036 },
            /* sp07 */ {   7,       1,         3,       0x73703037 },
            /* sp08 */ {   8,       2,         0,       0x73703038 },
            /* sp09 */ {   9,       2,         1,       0x73703039 },
            /* sp10 */ {   10,      2,         2,       0x73703130 },
            /* sp11 */ {   11,      2,         3,       0x73703131 },
            /* sp12 */ {   12,      3,         0,       0x73703132 },
            /* sp13 */ {   13,      3,         1,       0x73703133 },
            /* sp14 */ {   14,      3,         2,       0x73703134 },
            /* sp15 */ {   15,      3,         3,       0x73703135 },
            /* bp00 */ {   16,      4,         0,       0x62703030 },
            /* bp01 */ {   17,      4,         1,       0x62703031 },
            /* bp02 */ {   18,      4,         2,       0x62703032 },
            /* dp01 */ {   19,      4,         3,       0x64703031 },
        };

        return nodes[id].iRoilNodeId;
    }
}