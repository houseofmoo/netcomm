#include "shm_recv_worker.h"
#include <eROIL/print.h>
#include "types/types.h"

namespace eroil::worker {
    ShmRecvWorker::ShmRecvWorker(Router& router, 
                                 Label label, 
                                 size_t label_size)
        : m_router(router), m_label(label), m_label_size(label_size), m_recv_shm(nullptr) {}

    void ShmRecvWorker::run() {
        try {
            std::vector<std::byte> tmp;
            tmp.resize(m_label_size + sizeof(LabelHeader));
            while (!stop_requested()) {
                auto route = m_router.get_recv_route(m_label);
                if (route == nullptr) {
                    ERR_PRINT("route no longer exists, recv shm worker exiting for label=", m_label);
                    break;
                }

                if (auto* ptr = std::get_if<LocalPublisher>(&route->publisher)) {
                    auto evt_err = ptr->publish_event->wait(); // blocks
                    if (evt_err != evt::NamedEventErr::None) {
                        ERR_PRINT("named event err: ", (int)evt_err)
                    }
                    if (stop_requested()) break;

                    auto shm = m_router.get_recv_shm(m_label);
                    if (shm == nullptr) {
                        ERR_PRINT("recv shm block no longer exists, recv shm worker exiting for label=", m_label);
                        break;
                    }

                    auto err = shm->read(tmp.data(), tmp.size());
                    if (err != shm::ShmOpErr::None) {
                        // either interrupted or underlying error
                        ERR_PRINT("shm recv worker got error waiting for shm recv for label=", m_label, " errno=", (int)err);
                        continue;
                    }

                    // move pointer passed the header, only we care about the header, consumers do not
                    auto* label_ptr = tmp.data() + sizeof(LabelHeader);
                    m_router.recv_from_publisher(m_label, label_ptr, tmp.size() - sizeof(LabelHeader));
                } else {
                    ERR_PRINT("route is not a local route, recv shm worker exiting for label=", m_label);
                    break;
                }
            }
        } catch (const std::exception& e) {
            ERR_PRINT("shm recv worker got exception, worker stopping: ", e.what());
            // TODO: this thread is exiting, maybe because the wait event was removed
        } catch (...) {
            ERR_PRINT("shm recv worker for unknown exception, worker stopping");
            // TODO: this thread is exiting, maybe because the wait event was removed
        }
    }
}


