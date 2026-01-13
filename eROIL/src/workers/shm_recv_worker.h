#pragma once

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
            std::shared_ptr<shm::Shm> m_recv_shm;

        public:
            ShmRecvWorker(Router& router, Label label, size_t label_size);

            // do not copy
            ShmRecvWorker(const ShmRecvWorker&) = delete;
            ShmRecvWorker& operator=(const ShmRecvWorker&) = delete;

        protected:
            void run() override;
    };
}


