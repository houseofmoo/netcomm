#include "dispatch.h"

#include <cstring>
#include <eROIL/print.h>
#include "types/types.h"
#include "platform/platform.h"
#include "assertion.h"
#include "rtos.h"

namespace eroil {
    SendResult Dispatcher::dispatch_send_targets(const SendTargets& targets, io::SendBuf send_buf) const {
        bool failed = false;
        SendResult result{ SendOpErr::None, shm::ShmOpErr::None, {} };

        // local
        if (targets.has_local) {
            DB_ASSERT(!targets.shm.empty(), "dispatch_send_targets(): unexpected empty shm list");
            for (const auto& shm : targets.shm) {
                auto shm_err = shm->write_data(shm::ShmSendPayload{
                    targets.source_id,
                    targets.label,
                    0,
                    send_buf.total_size,
                    std::move(send_buf.data)
                });

                // TODO: handle error properly
                if (shm_err != shm::ShmSendErr::None) {
                     ERR_PRINT("shm send for label=", targets.label, ", errorcode=", (int)shm_err);
                }
            }
        }


        // remote
        if (targets.has_remote) {
            DB_ASSERT(!targets.sockets.empty(), "dispatch_send_targets(): unexpected empty socket list");
            for (const auto& sock : targets.sockets) {
                // dont bother sending to disconnected sockets
                // we're trying to reconnect in in comms handler
                if (sock == nullptr) continue;
                if (!sock->is_connected()) continue;
                
                auto sock_err = sock->send(send_buf.data.get(), send_buf.total_size);
                if (sock_err.code != sock::SockErr::None) {
                    failed = true;
                    result.sock_err.emplace(
                        sock->get_destination_id(),
                        sock_err
                    );
                    ERR_PRINT(
                        "socket send for label=", targets.label, 
                        ", errorcode=", (int)sock_err.code, ", op=", (int)sock_err.op
                    );
                }
            }
        }

        // write send IOSB for sender
        if (targets.publisher != nullptr) { 
            // ... there may not be a iosb
            if (targets.publisher->iosb != nullptr && targets.publisher->num_iosb > 0) {
                iosb::SendIosb* iosb = targets.publisher->iosb + targets.publisher->iosb_index;

                iosb->Status = failed ? -1 : 0;  // -1 is error, 0 is ok
                iosb->Reserve1 = 0;
                iosb->Header_Valid = 1;
                iosb->Reserve2 = static_cast<int>(iosb::RoilAction::SEND);
                iosb->Reserve3 = 0;
                iosb->pMsgAddr = static_cast<char*>(send_buf.data_src_addr);
                iosb->MsgSize = send_buf.data_size;
                iosb->Reserve4 = 0;
                iosb->Reserve5 = 0;
                iosb->Reserve6 = 0;
                iosb->Reserve7 = 0;
                iosb->Reserve8 = 0;
                iosb->Reserve9 = 0;
                iosb->Reserve10 = 0;
                iosb->TimeStamp = RTOS_Current_Time_Raw();

                iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                iosb->FC_Header.Destination_ID = targets.label;
                iosb->FC_Header.Source_ID = targets.source_id;

                targets.publisher->iosb_index = (targets.publisher->iosb_index + 1) % targets.publisher->num_iosb;
            }
            
            plat::try_signal_sem(targets.publisher->sem);
        }

        result.send_err = failed ? SendOpErr::Failed : SendOpErr::None;
        return result;
    }

    void Dispatcher::dispatch_recv_targets(const RecvTargets& targets,
                                           const std::byte* buf,
                                           const size_t size,
                                           const size_t recv_offset) const {

        
        for (const auto& subs : targets.subscribers) {
            // TODO: we need to write filed into recv IOSB when something goes wrong
            if (subs == nullptr) {
                ERR_PRINT(__func__, "(): got null subscriber for label=", targets.label);
                continue;
            }

            if (subs->buf == nullptr) { 
                ERR_PRINT(__func__, "(): subscriber has no buffer for label=", targets.label);
                continue;
            }

            if (subs->buf_slots == 0) {
                ERR_PRINT(__func__, "(): subscriber has no buffer slots for label=", targets.label);
                continue;
            }

            if (subs->buf_size == 0) { 
                ERR_PRINT(__func__, "(): subscriber buffer size is 0 for label=", targets.label);
                continue;
            }

            if (recv_offset > subs->buf_size) {
                ERR_PRINT(__func__, "(): recv offset > buf size for label=", targets.label);
                continue;
            }

            const size_t slot = subs->buf_index % subs->buf_slots;
            std::byte* dst = subs->buf + (slot * subs->buf_size);
            std::memcpy(dst + recv_offset, buf, size);

            // write recvrs IOSB 
            if (subs->iosb != nullptr && subs->num_iosb > 0) {
                iosb::ReceiveIosb* iosb = subs->iosb + subs->iosb_index;

                iosb->Status = 0; // -1 is error, 0 is ok
                iosb->Reserve1 = 0;
                iosb->Header_Valid = 1;
                iosb->Reserve2 = static_cast<int>(iosb::RoilAction::RECEIVE);
                iosb->Reserve3 = 0;
                iosb->MsgSize = size / 4; // they want it in words for some reason
                iosb->Reserve4 = 0;
                iosb->Messaage_Slot = subs->buf_index;
                iosb->Reserve5 = targets.label;
                iosb->pMsgAddr = reinterpret_cast<char*>(dst);
                iosb->Reserve6 = 0;
                iosb->Reserve7 = 0;
                iosb->Reserve8 = 0;
                iosb->E_SOF = 0;
                iosb->E_EOF = 0;
                iosb->TimeStamp = RTOS_Current_Time_Raw();

                iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                //iosb->FC_Header.Source_ID = m_id;
                //iosb->FC_Header.Destination_ID = (iForwardLabel == -1) ? targets.label : iForwardLabel;
                //iosb->FC_Header.Parameter = iOffsetInBytes; specifically the recvoffset (recv'd from a send header recvoffset)
                
                subs->iosb_index = (subs->iosb_index + 1) % subs->num_iosb;
            }
            
            subs->buf_index = (subs->buf_index + 1) % subs->buf_slots;

            // TODO: signal based on the signal mode?
            //if subs->signal_mode == ALWAYS ??
            int count = 0;
            std::memcpy(&count, buf, sizeof(count));
            LOG("signaling for val: ", count);
            plat::try_signal_sem(subs->sem);
        }
    }
}
