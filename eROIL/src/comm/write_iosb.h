#pragma once
#include <memory>
#include "types/const_types.h"
#include "types/handles.h"
#include "types/iosb.h"
#include "rtos.h"
#include "platform/platform.h"

namespace eroil::comm {
    inline void write_recv_iosb(std::shared_ptr<hndl::OpenReceiveData> sub,
                                const NodeId source_id,
                                const Label label,
                                const size_t size,
                                const size_t recv_offset,
                                std::byte* dst_buf_addr) {

        if (sub->iosb == nullptr || sub->num_iosb <= 0) {
            return;
        }
    
        // TODO: set status appropriately
        // values are 0...4 i think, and 0 means ok?

        iosb::ReceiveIosb* iosb = sub->iosb + sub->iosb_index;
        iosb->Status = 0;
        iosb->Reserve1 = 0;
        iosb->Header_Valid = 1;
        iosb->Reserve2 = static_cast<int32_t>(iosb::RoilAction::RECEIVE);
        iosb->Reserve3 = 0;
        iosb->MsgSize = static_cast<int32_t>(size / 4); // they want it in words for some reason
        iosb->Reserve4 = 0;
        iosb->Messaage_Slot = static_cast<int32_t>(sub->buf_index);
        iosb->Reserve5 = label;
        iosb->pMsgAddr = reinterpret_cast<char*>(dst_buf_addr);
        iosb->Reserve6 = 0;
        iosb->Reserve7 = 0;
        iosb->Reserve8 = 0;
        iosb->E_SOF = 0;
        iosb->E_EOF = 0;
        iosb->TimeStamp = RTOS_Current_Time_Raw();

        iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        iosb->FC_Header.Source_ID = source_id;
        iosb->FC_Header.Destination_ID = label;
        iosb->FC_Header.Parameter = static_cast<int32_t>(recv_offset);
        
        sub->iosb_index = (sub->iosb_index + 1) % sub->num_iosb;
    }

    inline void write_send_iosb(std::shared_ptr<hndl::OpenSendData> pub,
                                const NodeId source_id,
                                const Label label,
                                const size_t size,
                                const uint32_t fail_count,
                                void* src_buf_addr) {

            if (pub == nullptr) {
                return;
            }

            if (pub->iosb == nullptr || pub->num_iosb == 0) {
                return;
            }

            // TODO: set status appropriately
            // values are 0...4 i think, and 0 means ok?
            int32_t status = 0;
            if (fail_count > 0) {
                status = -1;
            }

            iosb::SendIosb* iosb = pub->iosb + pub->iosb_index;
            iosb->Status = status; 
            iosb->Reserve1 = 0;
            iosb->Header_Valid = 1;
            iosb->Reserve2 = static_cast<int>(iosb::RoilAction::SEND);
            iosb->Reserve3 = 0;
            iosb->pMsgAddr = static_cast<char*>(src_buf_addr);
            iosb->MsgSize = static_cast<int32_t>(size);
            iosb->Reserve4 = 0;
            iosb->Reserve5 = 0;
            iosb->Reserve6 = 0;
            iosb->Reserve7 = 0;
            iosb->Reserve8 = 0;
            iosb->Reserve9 = 0;
            iosb->Reserve10 = 0;
            iosb->TimeStamp = RTOS_Current_Time_Raw();

            iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            iosb->FC_Header.Source_ID = source_id;
            iosb->FC_Header.Destination_ID = label;
    }
}