#pragma once
#include <mutex>
#include <memory>
#include <utility>
#include "router/router.h"
#include "shm/shm_recv.h"
#include "types/const_types.h"
#include "macros.h"

namespace eroil::wrk {
    class ShmRecvWorker {
        private:
            rt::Router& m_router;
            NodeId m_id;
            std::shared_ptr<shm::ShmRecv> m_shm;

            std::atomic<bool> m_stop{false};
            std::thread m_thread;
            
            const int64_t MAX_TIMEOUT_MS = 50;

        public:
            ShmRecvWorker(rt::Router& router, NodeId id);
            ~ShmRecvWorker() { stop(); }

            EROIL_NO_COPY(ShmRecvWorker)
            EROIL_NO_MOVE(ShmRecvWorker)

            void start();
            void stop();

        private:
            bool stop_requested() const { return m_stop.load(std::memory_order_acquire); }
            void run();
            std::pair<bool, shm::ShmRecvData> get_next_record();
    };
}