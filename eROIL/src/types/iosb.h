#pragma once

#include "types/types.h"

namespace eroil {
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

    struct FcHeader {
        int R_CTL : 8;
        int Destination_ID : 24;
        int Priority : 7;
        int Preemption : 1;
        int Source_ID : 24;
        int DS_Type : 8;
        int F_CTL : 24;
        int SEQ_ID : 8;
        int DF_CTL : 8;
        int SEQ_CNT : 16;
        int OX_ID : 16;
        int RX_ID : 16;
        int Parameter : 32;
    };

    struct SendIosb {
        int Status : 17;
        int Reserve1 : 6;
        unsigned int Header_Valid : 1;
        int Reserve2 : 8;
        int Reserve3 : 32;
        char* pMsgAddr;
        int MsgSize : 32;
        int Reserve4 : 15;
        int Reserve5 : 1;
        int Reserve6 : 16;
        int Reserve7 : 32;
        int Reserve8 : 32;
        FcHeader FC_Header;
        int Reserve9 : 8;
        int Reserve10 : 24;
        uint64_t TimeStamp;
    };

    struct ReceiveIosb {
        int Status : 17;
        int Reserve1 : 6;
        unsigned int Header_Valid : 1;
        int Reserve2 : 8;
        int Reserve3 : 32;
        int MsgSize : 24;
        int Reserve4 : 16;
        int Messaage_Slot : 16;
        int Reserve5 : 32;
        char* pMsgAddr;
        int Reserve6 : 32;
        int Reserve7 : 32;
        FcHeader FC_Header;
        int Reserve8 : 26;
        int E_SOF : 3;
        int E_EOF : 3;
        uint64_t TimeStamp;
    };

    struct Iosb {
        int Status : 17;
        int Reserve1 : 6;
        int Header_Valid : 1;
        int Reserve2 : 8;
        int Reserve3 : 32;
        int Reserve4 : 32;
        int Reserve5 : 32;
        int Reserve6 : 32;
        int Reserve7 : 32;
        int Reserve8 : 32;
        FcHeader FC_Header;
        int Reserve9 : 32;
        uint64_t TimeStamp;
    };
}