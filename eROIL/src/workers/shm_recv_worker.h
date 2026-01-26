#pragma once
#include <mutex>
#include "thread_worker.h"
#include "router/router.h"
#include "shm/shm_recv.h"
#include "types/types.h"
#include "timer/timer.h"

namespace eroil::worker {
    class ShmRecvWorker final : public ThreadWorker {
        private:
            Router& m_router;
            time::Timer m_timeout;

        public:
            ShmRecvWorker(Router& router);

            // do not copy
            ShmRecvWorker(const ShmRecvWorker&) = delete;
            ShmRecvWorker& operator=(const ShmRecvWorker&) = delete;

        protected:
            void run() override;
            void on_stop_requested() override;
    };
}