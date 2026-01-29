#pragma once
#include "types/send_io_types.h"

namespace eroil::wrk {
    struct ShmSendPlan {
        static const auto& receivers(const io::SendJob& job) noexcept { return job.local_recvrs; }
        static auto& fail_count(io::SendJob& job) noexcept { return job.local_failure_count; }
        static bool is_local() noexcept { return true; }
        static bool is_remote() noexcept { return false; }

        static bool send_one(shm::ShmSend& shm, io::SendJob& job) noexcept {
            shm::ShmSendErr err = shm.send(
                job.source_id, 
                job.label, 
                job.seq,
                job.send_buffer.total_size,
                job.send_buffer.data.get()
            );
            
            if (err != shm::ShmSendErr::None) {
                // TODO: is there something to handle here?
                ERR_PRINT("shm send for label=", job.label, ", errorcode=", (int)err);
            }

            return err == shm::ShmSendErr::None;
        }
    };

    struct TcpSendPlan {
        static const auto& receivers(const io::SendJob& job) noexcept { return job.remote_recvrs; }
        static auto& fail_count(io::SendJob& job) noexcept { return job.remote_failure_count; }
        static bool is_local() noexcept { return false; }
        static bool is_remote() noexcept { return true; }

        static bool send_one(sock::TCPClient& sock, io::SendJob& job) noexcept {
            if (!sock.is_connected()) return false; // re-connection is being attempted in the background
            sock::SockResult result = sock.send(
                job.send_buffer.data.get(),
                job.send_buffer.total_size
            );
            
            if (result.code != sock::SockErr::None) {
                // TODO: is there something to handle here?
                ERR_PRINT("sokcet send for label=", job.label, ", errorcode=", (int)result.code);
            }

            return result.code == sock::SockErr::None;
        }
    };
}