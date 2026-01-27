#pragma once
#include <memory>
#include <vector>
#include <atomic>
#include <eROIL/print.h>
#include "rtos.h"
#include "platform/platform.h"
#include "shm/shm_send.h"
#include "socket/tcp_socket.h"
#include "handles.h"
#include "const_types.h"
#include "assertion.h"

namespace eroil::io {
     struct SendBuf {
        void* data_src_addr = nullptr; // where the data was copied from (for send IOSB)
        std::unique_ptr<std::byte[]> data = nullptr;
        std::size_t data_size = 0;
        std::size_t total_size = 0;  // size of data + header

        SendBuf(void* src_buf, const std::size_t size) : data_src_addr(src_buf) {
            DB_ASSERT(data_src_addr != nullptr, "cannot have nullptr src addr for send label");
            DB_ASSERT(size != 0, "cannot have 0 data size for send label");

            data_size = size;
            total_size = size + sizeof(LabelHeader);
            data = std::make_unique<std::byte[]>(total_size);
        }

        // move ok
        SendBuf(SendBuf&&) noexcept = default;
        SendBuf& operator=(SendBuf&&) noexcept = default;

        // copy deleted
        SendBuf(const SendBuf&) = delete;
        SendBuf& operator=(const SendBuf&) = delete;
    };

    struct SendJob {
        NodeId source_id;
        Label label;
        SendBuf send_buffer;
        std::shared_ptr<hndl::OpenSendData> publisher;
        
        uint32_t local_failure_count;
        std::vector<std::shared_ptr<shm::ShmSend>> local_recvrs;

        uint32_t remote_failure_count;
        std::vector<std::shared_ptr<sock::TCPClient>> remote_recvrs;

        std::atomic<uint32_t> pending_sends{0};

        explicit SendJob(SendBuf&& buf) :
            source_id{INVALID_NODE},
            label{INVALID_LABEL},
            send_buffer(std::move(buf)),
            publisher{nullptr},
            local_failure_count{0},
            local_recvrs{},
            remote_failure_count{0},
            remote_recvrs{},
            pending_sends{0} {}

        void complete_one() noexcept {
            const uint32_t prev = pending_sends.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 0) {
                // TODO: underflowed somehow
                ERR_PRINT("pending sends underflowed");
                return;
            }

            if (prev == 1) { // we were the last sender, write the iosb
                finalize_iosb();
            }
        }

        void finalize_iosb() noexcept {
            // write IOSB using status
            if (publisher == nullptr) {
                return;
            }

            if (publisher->iosb == nullptr || publisher->num_iosb == 0) {
                return;
            }

            int32_t status = 0;
            if (remote_failure_count > 0 || local_failure_count > 0) {
                status = -1;
            }

            auto* iosb = publisher->iosb + publisher->iosb_index;
            iosb->Status = status;  // -1 is error, 0 is ok
            iosb->Reserve1 = 0;
            iosb->Header_Valid = 1;
            iosb->Reserve2 = static_cast<int>(iosb::RoilAction::SEND);
            iosb->Reserve3 = 0;
            iosb->pMsgAddr = static_cast<char*>(send_buffer.data_src_addr);
            iosb->MsgSize = send_buffer.data_size;
            iosb->Reserve4 = 0;
            iosb->Reserve5 = 0;
            iosb->Reserve6 = 0;
            iosb->Reserve7 = 0;
            iosb->Reserve8 = 0;
            iosb->Reserve9 = 0;
            iosb->Reserve10 = 0;
            iosb->TimeStamp = RTOS_Current_Time_Raw();

            iosb->FC_Header = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            iosb->FC_Header.Destination_ID = label;
            iosb->FC_Header.Source_ID = source_id;

            publisher->iosb_index = (publisher->iosb_index + 1) % publisher->num_iosb;

            plat::try_signal_sem(publisher->sem);
        }
    };

    struct JobCompleteGuard {
        std::shared_ptr<SendJob> job;
        ~JobCompleteGuard() { if (job != nullptr) job->complete_one(); }
    };

    struct ShmSendPlan {
        static const auto& receivers(const SendJob& job) noexcept { return job.local_recvrs; }
        static auto& fail_count(SendJob& job) noexcept { return job.local_failure_count; }
        static bool is_local() noexcept { return true; }
        static bool is_remote() noexcept { return false; }

        static bool send_one(shm::ShmSend& shm, SendJob& job) noexcept {
            auto err = shm.send(
                job.source_id, 
                job.label, 
                0,
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
        static const auto& receivers(const SendJob& job) noexcept { return job.remote_recvrs; }
        static auto& fail_count(SendJob& job) noexcept { return job.remote_failure_count; }
        static bool is_local() noexcept { return false; }
        static bool is_remote() noexcept { return true; }

        static bool send_one(sock::TCPClient& sock, SendJob& job) noexcept {
            if (!sock.is_connected()) return false; // re-connection is being attempted in the background
            auto err = sock.send(
                job.send_buffer.data.get(),
                job.send_buffer.total_size
            );
            
            if (err.code != sock::SockErr::None) {
                // TODO: is there something to handle here?
                ERR_PRINT("sokcet send for label=", job.label, ", errorcode=", (int)err.code);
            }

            return err.code == sock::SockErr::None;
        }
    };
}