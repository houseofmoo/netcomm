#pragma once
#include <mutex>
#include "router/router.h"
#include "shm/shm_recv.h"
#include "types/const_types.h"
#include "timer/timer.h"
#include "types/macros.h"

namespace eroil::worker {
    class ShmRecvWorker {
        private:
            Router& m_router;
            time::Timer m_timeout;

            std::atomic<bool> m_stop{false};
            std::thread m_thread;

        public:
            ShmRecvWorker(Router& router);
            ~ShmRecvWorker() { stop(); }

            EROIL_NO_COPY(ShmRecvWorker)
            EROIL_NO_MOVE(ShmRecvWorker)

            void start();
            void stop();

        private:
            bool stop_requested() const { return m_stop.load(std::memory_order_acquire); }
            void run();
    };
}