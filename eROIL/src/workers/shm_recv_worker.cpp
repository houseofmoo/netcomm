#include "shm_recv_worker.h"
#include <eROIL/print.h>
#include "types/types.h"
#include "log/evtlog_api.h"

namespace eroil::worker {
    ShmRecvWorker::ShmRecvWorker(Router& router, 
                                 Label label, 
                                 size_t label_size)
        : m_router(router), m_label(label), m_label_size(label_size), m_curr_event(nullptr) {}

    void ShmRecvWorker::run() {
        try {
            // size our recv block
            const size_t hdr_size = sizeof(LabelHeader);

            std::vector<std::byte> tmp;
            tmp.resize(m_label_size + hdr_size);


            while (!stop_requested()) {
                std::shared_ptr<evt::NamedEvent> ev;
                {
                    auto route = m_router.get_recv_route(m_label);
                    if (route == nullptr) {
                        ERR_PRINT("recv worker label=", m_label, " no longer has a route, worker exits");
                        break;
                    }

                    auto* pub = std::get_if<LocalPublisher>(&route->publisher);
                    if (pub == nullptr) {
                        ERR_PRINT("recv worker label=", m_label, " route is not local, worker exits");
                        break;
                    }

                    ev = pub->publish_event;
                    if (ev == nullptr) {
                        ERR_PRINT("recv worker label=", m_label, " has no publish event, worker exits");
                        break;
                    }

                    {
                        std::lock_guard lock(m_event_mtx);
                        m_curr_event = ev;
                    }
                }
                
                // a stop request may have occured before we set curr event
                if (stop_requested()) break;

                // blocks forever until signaled
                auto evt_err = ev->wait(); 
                evtlog::info(elog_kind::ShmRecvWorker_Start, elog_cat::Worker, m_label);
                
                // release reference to event the moment we dont need it
                {
                    std::lock_guard lock(m_event_mtx);
                    m_curr_event.reset();
                }

                // make sure a stop request did not arrive while blocking
                if (stop_requested()) break;

                if (evt_err == evt::NamedEventErr::Timeout) {
                    // should never happen since we wait indefinitely
                    evtlog::info(elog_kind::ShmRecvWorker_End, elog_cat::Worker, m_label);
                    continue;
                }

                if (evt_err != evt::NamedEventErr::None) {
                    ERR_PRINT("recv worker label=", m_label, " wait() got err=", (int)evt_err, ", worker exits");
                    evtlog::error(elog_kind::ShmRecvWorker_Error, elog_cat::Worker, m_label, m_label_size);
                    break;
                }

                auto shm = m_router.get_recv_shm(m_label);
                if (shm == nullptr) {
                    ERR_PRINT("recv worker label=", m_label, " has no shm block to read, worker exits");
                    evtlog::error(elog_kind::ShmRecvWorker_Error, elog_cat::Worker, m_label, m_label_size);
                    break;
                }

                auto err = shm->read(tmp.data(), tmp.size());
                if (err != shm::ShmOpErr::None) {
                    ERR_PRINT("recv worker label=", m_label, " error reading shm block, worker continues");
                    evtlog::warn(elog_kind::ShmRecvWorker_Warning, elog_cat::Worker, m_label, m_label_size);
                    evtlog::info(elog_kind::ShmRecvWorker_End, elog_cat::Worker, m_label, m_label_size);
                    continue;
                }

                // move pointer passed the header
                auto* label_ptr = tmp.data() + hdr_size;
                m_router.recv_from_publisher(m_label, label_ptr, tmp.size() - hdr_size);
                evtlog::info(elog_kind::ShmRecvWorker_End, elog_cat::Worker, m_label, m_label_size);
            }
        } catch (const std::exception& e) {
            ERR_PRINT("shm recv worker for label=", m_label, " got exception, worker stopping: ", e.what());
            // TODO: this thread is exiting, maybe because the wait event was removed
        } catch (...) {
            ERR_PRINT("shm recv worker for label=", m_label, " got unknown exception, worker stopping");
            // TODO: this thread is exiting, maybe because the wait event was removed
        }

        // on exit release the current event if its still held
        {
            std::lock_guard lock(m_event_mtx);
            m_curr_event.reset();
        }
        
        evtlog::info(elog_kind::ShmRecvWorker_Exit, elog_cat::Worker, m_label, m_label_size);
    }

    void ShmRecvWorker::on_stop_requested() {
        // worker inherits atomic bool "m_stop" that is 
        // set before this function is called
        std::shared_ptr<evt::NamedEvent> ev;
        {
            std::lock_guard lock(m_event_mtx);
            ev = m_curr_event;
        }

        if (ev != nullptr) {
            // wake worker so they see the stop request
            ev->post();
        }
    }
}


