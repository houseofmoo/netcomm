#include "dispatcher.h"

#include <cstring>
#include <eROIL/print.h>
#include "platform.h"

namespace eroil {
    bool Dispatcher::validate_recv_target(const OpenReceiveData& data) {
        if (data.buf == nullptr) return false;
        if (data.buf_slots == 0) return false;
        if (data.buf_size == 0) return false;
        return true;
    }

    SendOpErr Dispatcher::dispatch_send(const SendPlan& plan,
                                        const SendTargets& targets,
                                        const void* buf,
                                        size_t size) const {
        if (buf == nullptr || size == 0) return SendOpErr::Failed;

        if (size != plan.label_size) {
            ERR_PRINT("dispatch_send(): size mismatch label=", plan.label,
                      " expected=", plan.label_size, " got=", size);
            return SendOpErr::SizeMismatch;
        }

        bool failed = false;

        // local
        if (plan.want_local) {
            if (!targets.shm) {
                failed = true;
            } else {
                auto err = targets.shm->send(buf, size);
                if (err != shm::ShmOpErr::None) failed = true;
            }
        }

        // remote
        if (!plan.remote_node_ids.empty()) {
            if (targets.sockets.empty()) {
                failed = true; // couldn't resolve any socket
            }

            for (const auto& sock : targets.sockets) {
                if (!sock) {
                    failed = true;
                    continue;
                }
                sock->send(buf, size); // TODO: socket send return an error
            }

            if (targets.sockets.size() < plan.remote_node_ids.size()) {
                failed = true;
            }
        }

        return failed ? SendOpErr::Failed : SendOpErr::None;
    }

    void Dispatcher::dispatch_recv_targets(Label label,
                                       size_t label_size,
                                       const void* buf,
                                       size_t size,
                                       const std::vector<std::shared_ptr<RecvTarget>>& targets) const {
        if (!buf || size == 0) return;
        if (size != label_size) {
            ERR_PRINT("dispatch_recv_targets(): size mismatch label=", label,
                    " expected=", label_size, " got=", size);
            return;
        }

        for (const auto& target : targets) {
            if (target == nullptr) continue;

            OpenReceiveData& data = target->data;
            if (!validate_recv_target(data)) continue;

            // Prevent overflow of the receiver stride
            if (data.buf_size != size) {
                ERR_PRINT("dispatch_recv_targets(): buf_size mismatch label=", label,
                        " expected buf_size=", data.buf_size, " got=", size);
                continue;
            }

            const size_t slot = data.buf_index % data.buf_slots;
            uint8_t* dst = data.buf + (slot * data.buf_size);
            std::memcpy(dst, buf, size);
            data.buf_index = (data.buf_index + 1) % data.buf_slots;

            if (data.iosb != nullptr && data.num_iosb > 0) {
                // TODO: write recv IOSB
            }

            signal_recv_sem(data.sem, data.signal_mode);
        }
    }
}
