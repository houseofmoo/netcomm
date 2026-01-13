#include "dispatcher.h"

#include <cstring>
#include <eROIL/print.h>
#include "types/types.h"
#include "platform.h"

namespace eroil {
    SendResult Dispatcher::dispatch_send(const SendTargets& targets,
                                        const void* buf,
                                        size_t size) const {
        bool failed = false;
        SendResult result{};

        // local
        if (targets.shm != nullptr) {
            auto shm_err = targets.shm->write(buf, size);
            if (shm_err != shm::ShmOpErr::None) {
                result.shm_err = shm_err;
                failed = true;
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

                // dont bother sending to disconnected sockets
                // we're trying to reconnect in in comms handler
                if (!sock->is_connected()) continue;
                
                auto sock_err = sock->send(buf, size);
                if (sock_err.code != sock::SockErr::None) {
                    failed = true;
                    result.sock_err.emplace(
                        sock->get_destination_id(),
                        sock_err
                    );
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

        result.send_err = failed ? SendOpErr::Failed : SendOpErr::None;
        return result;
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
