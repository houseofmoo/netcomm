#pragma once
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <eROIL/print.h>
#include "rtos.h"
#include "platform/platform.h"
#include "shm/shm_send.h"
#include "socket/tcp_socket.h"
#include "handles.h"
#include "const_types.h"
#include "label_io_types.h"
#include "assertion.h"
#include "types/macros.h"
#include "comm/write_iosb.h"

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
        
        EROIL_NO_COPY(SendBuf)
        EROIL_DEFAULT_MOVE(SendBuf)
    };

    enum class SendJobErr {
        None,
        RouteNotFound,
        SizeMismatch,
        SizeTooLarge,
        ShmMissing,
        SocketMissing,
        UnknownHandle,
        NoPublishers,
        IncorrectPublisher,
        Failed
    };

    struct SendJob {
        NodeId source_id;
        Label label;
        SendBuf send_buffer;
        uint32_t seq;
        std::shared_ptr<hndl::SendHandle> publisher;
        
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

        EROIL_NO_COPY(SendJob)
        EROIL_NO_MOVE(SendJob)

        void complete_one() noexcept {
            const uint32_t prev = pending_sends.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 0) {
                ERR_PRINT("pending sends underflowed");
                return;
            }

            if (prev == 1) { // we were the last sender, write the iosb
                finalize_iosb();
            }
        }

        void finalize_iosb() noexcept {
            std::lock_guard lock(publisher->mtx);
            comm::write_send_iosb(
                publisher.get(), 
                source_id, 
                label, 
                send_buffer.data_size,
                local_failure_count + remote_failure_count,
                send_buffer.data_src_addr
            );
            plat::try_signal_sem(publisher->data.sem);
        }
    };

    struct JobCompleteGuard {
        std::shared_ptr<SendJob> job;
        JobCompleteGuard() = delete;
        explicit JobCompleteGuard(std::shared_ptr<SendJob> j) : job(j) {}
        ~JobCompleteGuard() { if (job != nullptr) job->complete_one(); }
        EROIL_NO_COPY(JobCompleteGuard)
        EROIL_NO_MOVE(JobCompleteGuard)
    };
}