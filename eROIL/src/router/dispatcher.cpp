#include "dispatcher.h"

#include <cstring>
#include <eROIL/print.h>
#include "types/types.h"
#include "types/label_hdr.h"
#include "platform.h"

namespace eroil {
    SendOpErr Dispatcher::dispatch_send(const SendTargets& targets,
                                        const void* buf,
                                        size_t size) const {
        bool failed = false;

        // local
        if (targets.shm != nullptr) {
            auto shm_err = targets.shm->write(buf, size);
            if (shm_err != shm::ShmOpErr::None) {
                ERR_PRINT("shared memory write error=", (int)shm_err);
            }

            for (auto evt : targets.shm_signals) {
                if (evt == nullptr) {
                    ERR_PRINT("shared memory named event was null");
                    continue;
                }
                evt->post();
            }
        }

        // remote
        if (!targets.sockets.empty()) {
            for (const auto& sock : targets.sockets) {
                if (sock == nullptr) continue;
                auto sock_err = sock->send(buf, size);
                if (sock_err.code != sock::SockErr::None) {
                    if (sock_err.code == sock::SockErr::Closed) {
                        // TODO: tell router the socket is dead somehow
                    }
                    ERR_PRINT("socket send errorcode=", (int)sock_err.code, ", op=", (int)sock_err.op);
                }
            }
        }

        // write IOSB
        for (auto& pubs : targets.publishers) {
            if (pubs == nullptr) continue;

            if (pubs->iosb != nullptr && pubs->num_iosb > 0) {
                // TODO: write recv IOSB
                pubs->iosb_index = (pubs->iosb_index + 1) & pubs->num_iosb;
            }
        }

        return failed ? SendOpErr::Failed : SendOpErr::None;
    }

    void Dispatcher::dispatch_recv_targets(const RecvTargets& targets,
                                           const void* buf,
                                           size_t size) const {
        for (const auto& subs : targets.subscribers) {
            if (subs == nullptr) continue;

            if (subs->buf == nullptr) continue;
            if (subs->buf_slots == 0) continue;
            if (subs->buf_size == 0) continue;

            const size_t slot = subs->buf_index % subs->buf_slots;
            uint8_t* dst = subs->buf + (slot * subs->buf_size);
            std::memcpy(dst, buf, size);
            subs->buf_index = (subs->buf_index + 1) % subs->buf_slots;

            // write IOSB 
            if (subs->iosb != nullptr && subs->num_iosb > 0) {
                // TODO: write recv IOSB
                subs->iosb_index = (subs->iosb_index + 1) & subs->num_iosb;
            }

            signal_recv_sem(subs->sem, subs->signal_mode);
        }
    }
}
