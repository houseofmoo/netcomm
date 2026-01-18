#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

#include "types/types.h"
#include "types/handles.h"
#include "events/semaphore.h"
#include "router/router.h"


namespace eroil::worker {
    struct SendQEntry {
        handle_uid uid;
        Label label;
        SendBuf send_buf;
    };

    class SendWorker {
        private:
            Router& m_router;
            std::queue<SendQEntry> m_send_data_q;
            evt::Semaphore m_sem;
            std::mutex m_mtx;
            std::atomic<bool> continue_work{false};
            std::thread work_thread;

        public:
            explicit SendWorker(Router& router);
            ~SendWorker() = default;

            // do not copy
            SendWorker(const SendWorker&) = delete;
            SendWorker& operator=(const SendWorker&) = delete;

            void start();
            void stop();
            void enqueue(SendQEntry entry);

        private:
            void work();
    };
}