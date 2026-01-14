#include "workers/send_worker.h"
#include <string>
#include <optional>
#include <eROIL/print.h>

namespace eroil::worker {
    
    SendWorker::SendWorker(Router& router) : m_router(router) {}

    void SendWorker::start() {
        bool expected = false;
        if (!continue_work.compare_exchange_strong(expected, true)) {
            return; // someone tried to double start me
        }
        work_thread = std::thread([this]() { work(); });
    }

    void SendWorker::stop() {
        continue_work.store(false, std::memory_order_release);

        m_sem.post();
        if (work_thread.joinable()) {
            work_thread.join();
        }

        // pop all remaining data entries
        std::lock_guard lock(m_mtx);
        while (!m_send_data_q.empty()) m_send_data_q.pop();
    }

    void SendWorker::enqueue(SendQEntry entry) {
        // do not allow new work if continue work is false
        if (!continue_work.load(std::memory_order_acquire)) return;

        // do not allow bad data to enter queue
        if (entry.data == nullptr || entry.data_size == 0) {
            ERR_PRINT("tried to enqueue null or empty data");
            return;
        }

        {
            std::lock_guard lock(m_mtx);
            m_send_data_q.push(std::move(entry));
        }

        auto err = m_sem.post();
        if (err != evt::SemOpErr::None) {
            switch (err) {
                case evt::SemOpErr::MaxCountReached: {
                    ERR_PRINT("cannot enqueue anymore send jobs"); 
                    break;
                }
                case evt::SemOpErr::SysError: {
                    ERR_PRINT("semaphore post got system error");
                    break;
                }
                default: {
                    ERR_PRINT("emaphore post failed to unknown error, SemOpErr: ", std::to_string((int)err));
                    break;
                }
            }
        }
    }

    void SendWorker::work() {
        int wait_errs = 0;
        
        while (continue_work.load(std::memory_order_acquire)) {
            auto err = m_sem.wait();
            if (!continue_work.load(std::memory_order_acquire)) return;

            if (err != evt::SemOpErr::None) {
                ERR_PRINT("send worker got sem error waiting on work");
                if (++wait_errs > 10) {
                    continue_work.store(false, std::memory_order_release);
                    ERR_PRINT("send worker had too many sem errors in a row, exiting");
                    return;
                }
                continue;
            }
            wait_errs = 0;

            // drain queue
            while(true) {
                std::optional<SendQEntry> entry;
                {
                    std::lock_guard lock(m_mtx);
                    if (!m_send_data_q.empty()) {
                        entry.emplace(std::move(m_send_data_q.front()));
                        m_send_data_q.pop();
                    }
                }
                if (!entry) break;

                try {
                    auto result = m_router.send_to_subscribers(entry->label, entry->data.get(), entry->data_size, entry->uid);
                    switch (result.send_err) {
                        // validation error
                        case SendOpErr::RouteNotFound: // fallthrough
                        case SendOpErr::SizeMismatch:  // fallthrough
                        case SendOpErr::SizeTooLarge:  // fallthrough
                        case SendOpErr::NoPublishers: { break; }

                        // see why we failed
                        case SendOpErr::Failed: {
                            // TODO: handle failed send error
                        }

                        // no error
                        case SendOpErr::None: { break; }
                        default: { break; } // unknown error?
                    }
                } catch (const std::exception& e) {
                    ERR_PRINT("Exception in worker thread, entry.label: ", entry->label, ", exception: ", e.what());
                } catch (...) {
                    ERR_PRINT("Unknown exception in worker thread, entry.label: ", entry->label);
                }
            }
        }
    }
}