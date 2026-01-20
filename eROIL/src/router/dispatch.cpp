#include "dispatch.h"

#include <cstring>
#include <eROIL/print.h>
#include "types/types.h"
#include "platform/platform.h"
#include "assertion.h"
#include <cassert>

namespace eroil {
    SendResult Dispatcher::dispatch_send_targets(const SendTargets& targets, SendBuf send_buf) const {
        bool failed = false;
        SendResult result{ SendOpErr::None, shm::ShmOpErr::None, {} };

        // local
        if (targets.has_local) {
            DB_ASSERT(targets.shm != nullptr, "dispatch_send_targets(): unexpected nullptr to shm memory block");
            auto shm_err = targets.shm->write(send_buf.data.get(), send_buf.total_size);
            if (shm_err != shm::ShmOpErr::None) {
                result.shm_err = shm_err;
                failed = true;
                ERR_PRINT("shared memory write for label=", targets.label, "err=", (int)shm_err);
            }

            for (auto evt : targets.shm_signals) {
                if (evt == nullptr) {
                    ERR_PRINT("shared memory write named event was null for label=", targets.label);
                    continue;
                }
                evt->post();
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
                eroil::SendIosb* iosb = targets.publisher->iosb + targets.publisher->iosb_index;

                // TODO: we need a way to tell what buffer was used the buf in the IOSB
                // or a buffer that was provided, that way we can set iosb->pMsgAddr correctly
                // this should also be true for MsgSize (but this one should alway be the same)

                iosb->Status = send_buf.total_size;  // number of bytes sent is status (-1 is error, 0 is disconnected)
                iosb->Reserve1 = 0;
                iosb->Header_Valid = 1;
                iosb->Reserve2 = 0;  // 0 for send, 1 for receive (cause thats how they defined it)
                iosb->Reserve3 = 0;
                iosb->pMsgAddr = reinterpret_cast<char*>(send_buf.data_src_addr);
                iosb->MsgSize = send_buf.data_size;
                iosb->Reserve4 = 0;
                iosb->Reserve5 = 0;
                iosb->Reserve6 = 0;
                iosb->Reserve7 = 0;
                iosb->Reserve8 = 0;
                iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                iosb->Reserve9 = 0;
                iosb->Reserve10 = 0;
                iosb->TimeStamp = 0x12345678;  // TODO: write real timestamp here

                targets.publisher->iosb_index = (targets.publisher->iosb_index + 1) % targets.publisher->num_iosb;
            }
            
            plat::try_signal_sem(targets.publisher->sem);
        }

        result.send_err = failed ? SendOpErr::Failed : SendOpErr::None;
        return result;
    }

    void Dispatcher::dispatch_recv_targets(const RecvTargets& targets,
                                           const void* buf,
                                           size_t size) const {

        
        for (const auto& subs : targets.subscribers) {
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

            const size_t slot = subs->buf_index % subs->buf_slots;
            uint8_t* dst = subs->buf + (slot * subs->buf_size);
            std::memcpy(dst, buf, size);

            // write recvrs IOSB 
            if (subs->iosb != nullptr && subs->num_iosb > 0) {
                eroil::ReceiveIosb* iosb = subs->iosb + subs->iosb_index;

                iosb->Status = size; // number of bytes sent is status (-1 is error, 0 is disconnected)
                iosb->Reserve1 = 0;
                iosb->Header_Valid = 1;
                iosb->Reserve2 = 1;  // 0 for send, 1 for receive (cause thats how they defined it)
                iosb->Reserve3 = 0;
                iosb->MsgSize = 0;
                iosb->Reserve4 = 0;
                iosb->Messaage_Slot = subs->buf_index;
                iosb->Reserve5 = 0;
                iosb->pMsgAddr = reinterpret_cast<char*>(dst);
                iosb->Reserve6 = 0;
                iosb->Reserve7 = 0;
                iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                iosb->Reserve8 = 0;
                iosb->E_SOF = 0;
                iosb->E_EOF = 0;
                iosb->TimeStamp = 0; // TODO: write real timestamp here

                subs->iosb_index = (subs->iosb_index + 1) % subs->num_iosb;
            }
            
            subs->buf_index = (subs->buf_index + 1) % subs->buf_slots;

            plat::try_signal_sem(subs->sem, subs->signal_mode);
        }
    }
}
