#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

#include "types/const_types.h"
#include "events/semaphore.h"
#include "types/macros.h"
#include "log/evtlog_api.h"

namespace eroil::wrk {
    template <class SendPlan>
    class SendWorker {
        private:
            std::queue<std::shared_ptr<io::SendJob>> m_send_q;
            evt::Semaphore m_sem;
            std::mutex m_mtx;

            std::atomic<bool> m_stop{false};
            std::thread m_thread;

        public:
            explicit SendWorker() = default;
            ~SendWorker() { stop(); };

            EROIL_NO_COPY(SendWorker)
            EROIL_NO_MOVE(SendWorker)

            void enqueue(std::shared_ptr<io::SendJob> job) {
                if (stop_requested()) return;

                {
                    std::lock_guard lock(m_mtx);
                    m_send_q.push(job);
                }

                evt::SemOpErr err = m_sem.post();
                if (err != evt::SemOpErr::None) {
                    switch (err) {
                        case evt::SemOpErr::MaxCountReached: {
                            ERR_PRINT("cannot signal anymore send jobs"); 
                            break;
                        }
                        case evt::SemOpErr::SysError: {
                            ERR_PRINT("semaphore post got system error");
                            break;
                        }
                        default: {
                            ERR_PRINT("semaphore post failed to unknown error, SemOpErr: ", std::to_string((int)err));
                            break;
                        }
                    }
                }
            }

            void start() {
                if (m_thread.joinable()) return;
                m_stop.store(false, std::memory_order_release);
                m_thread = std::thread([this] { run(); });
            }

            void stop() {
                // NOTE: send workers (shm and socket) are expected to live for the lifetime of the
                // application. calling stop() SHOULD cleanly stop these threads, but I don't know
                // why anyone would want to do that
                
                bool was_stopping = m_stop.exchange(true, std::memory_order_acq_rel);
                if (!was_stopping) {
                    m_sem.post();
                }

                if (m_thread.joinable()) {
                    m_thread.join();
                }

                // pop all remaining data entries
                std::lock_guard lock(m_mtx);
                while (!m_send_q.empty()) m_send_q.pop();
            }

        private:
            bool stop_requested() const { return m_stop.load(std::memory_order_acquire); }

            void run() {
                while (!stop_requested()) {
                    if (m_sem.wait() != evt::SemOpErr::None) {
                        ERR_PRINT("send worker got sem error waiting on work");
                        continue;
                    }

                    if (stop_requested()) {
                        LOG("send worker got stop request, exiting");
                        break;
                    }

                    // drain queue of all jobs
                    while (true) {
                        std::shared_ptr<io::SendJob> job = nullptr;
                        {
                            std::lock_guard lock(m_mtx);
                            if (!m_send_q.empty()) {
                                job = std::move(m_send_q.front());
                                m_send_q.pop();
                            }
                        }

                        if (job == nullptr) break; // no jobs left

                        try {
                            EvtMark mark(elog_cat::SendWorker);
                            for (const auto& recvr : SendPlan::receivers(*job)) {
                                io::JobCompleteGuard guard{job};
                                if (recvr == nullptr) continue;
                                if (!SendPlan::send_one(*recvr, *job)) {
                                    evtlog::warn(elog_kind::SendFailed, elog_cat::SendWorker);
                                    ++SendPlan::fail_count(*job);
                                }
                            }
                            // IOSB is written by which ever sender completes last, occurs when
                            // job->pending_sends == 0
                        } catch (const std::exception& e) {
                            ERR_PRINT("send worker exception, label: ", job->label, ", exception: ", e.what());
                        } catch (...) {
                            ERR_PRINT("send worker unknown exception, label: ", job->label);
                        }
                    }
                }
            }
    };
}