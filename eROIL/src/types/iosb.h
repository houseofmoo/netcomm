#pragma once

#include "types/types.h"

namespace eroil::iosb {
    // ALL OF THESE MUST MATCH THE TYPES DEFINED IN ROIL
    enum class SignalMode {
        OVERWRITE = 0,
        BUFFER_FULL = 1,
        EVERY_MESSAGE = 2,
    };

    enum class RoilAction {
        SEND = 0x0,
        RECEIVE = 0x1,
    };

    enum class IoType {
        SLOT = 1,
        OFFSET = 2,
    };

    struct RTOSTime {
        unsigned int uiMsb;
        unsigned int uiLsb;
    };

    struct FcHeader {
        int32_t R_CTL : 8;
        int32_t Destination_ID : 24;
        int32_t Priority : 7;
        int32_t Preemption : 1;
        int32_t Source_ID : 24;
        int32_t DS_Type : 8;
        int32_t F_CTL : 24;
        int32_t SEQ_ID : 8;
        int32_t DF_CTL : 8;
        int32_t SEQ_CNT : 16;
        int32_t OX_ID : 16;
        int32_t RX_ID : 16;
        int32_t Parameter : 32;
    };

    struct SendIosb {
        int32_t Status : 17;
        int32_t Reserve1 : 6;
        uint32_t Header_Valid : 1;
        int32_t Reserve2 : 8; // send action
        int32_t Reserve3 : 32;
        char* pMsgAddr; // 64 bits
        int32_t MsgSize : 32;
        int32_t Reserve4 : 15;
        int32_t Reserve5 : 1;
        int32_t Reserve6 : 16;
        int32_t Reserve7 : 32;
        int32_t Reserve8 : 32;
        FcHeader FC_Header; // 6 * 32
        int32_t Reserve9 : 8;
        int32_t Reserve10 : 24;
        RTOSTime TimeStamp; // 64 bits
    };

    struct ReceiveIosb {
        int32_t Status : 17;
        int32_t Reserve1 : 6;
        uint32_t Header_Valid : 1;
        int32_t Reserve2 : 8; // send action
        int32_t Reserve3 : 8;
        int32_t MsgSize : 24;
        int32_t Reserve4 : 16;
        int32_t Messaage_Slot : 16;
        int32_t Reserve5 : 32; // label?
        char* pMsgAddr; // 64 bits
        int32_t Reserve6 : 32;
        int32_t Reserve7 : 32;
        FcHeader FC_Header; // 64 bits
        int32_t Reserve8 : 26;
        int32_t E_SOF : 3;
        int32_t E_EOF : 3;
        RTOSTime TimeStamp;
    };

    struct Iosb {
        int32_t Status : 17;
        int32_t Reserve1 : 6;
        int32_t Header_Valid : 1;
        int32_t Reserve2 : 8;
        int32_t Reserve3 : 32;
        int32_t Reserve4 : 32;
        int32_t Reserve5 : 32;
        int64_t Reserve6 : 64;
        int32_t Reserve7 : 32;
        int32_t Reserve8 : 32;
        FcHeader FC_Header;
        int32_t Reserve9 : 32;
        uint64_t TimeStamp;
    };

    static_assert(sizeof(Iosb) == sizeof(ReceiveIosb));
    static_assert(sizeof(Iosb) == sizeof(SendIosb));
}