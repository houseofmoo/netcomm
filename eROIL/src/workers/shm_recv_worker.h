#pragma once
#include <mutex>
#include "types/types.h"
#include "router/router.h"
#include "shm/shm.h"
#include "thread_worker.h"

namespace eroil::worker {
    class ShmRecvWorker final : public ThreadWorker {
        private:
            Router& m_router;
            Label m_label;
            size_t m_label_size;

            mutable std::mutex m_event_mtx;
            std::shared_ptr<evt::NamedEvent> m_curr_event;

        public:
            ShmRecvWorker(Router& router, Label label, size_t label_size);

            // do not copy
            ShmRecvWorker(const ShmRecvWorker&) = delete;
            ShmRecvWorker& operator=(const ShmRecvWorker&) = delete;

        protected:
            void run() override;
            void on_stop_requested() override;
    };
}


