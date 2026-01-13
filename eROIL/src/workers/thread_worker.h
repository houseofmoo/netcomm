#pragma once
#include <atomic>
#include <thread>

namespace eroil::worker {
    class ThreadWorker {
        private:
            std::atomic<bool> m_stop{false};
            std::thread m_thread;

        protected:
            bool stop_requested() const { return m_stop.load(std::memory_order_acquire); }
            virtual void run() = 0;
            virtual void on_stop_requested() {}
            virtual void on_stopped() {}

        public:
            ThreadWorker() = default;
            ThreadWorker(const ThreadWorker&) = delete;
            ThreadWorker& operator=(const ThreadWorker&) = delete;
            virtual ~ThreadWorker() = default; // must call stop() explicitly

            void start() {
                m_stop.store(false, std::memory_order_release);
                m_thread = std::thread([this] { run(); });
            }

            void stop() {
                bool was_stopping = m_stop.exchange(true, std::memory_order_acq_rel);
                if (!was_stopping) {
                    on_stop_requested();
                }

                if (m_thread.joinable()) {
                    m_thread.join();
                }

                on_stopped();
            }
    };
}
