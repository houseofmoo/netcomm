#include "shm_recv_worker.h"
#include <chrono>
#include <eROIL/print.h>
#include "types/types.h"
#include "log/evtlog_api.h"

namespace eroil::worker {
    ShmRecvWorker::ShmRecvWorker(Router& router) : m_router(router) {}

    void ShmRecvWorker::run() {
        try {
            const int64_t MAX_TIMEOUT_MS = 50;
            bool stop_runing = false;
            auto shm = m_router.get_recv_shm();
            if (shm == nullptr) {
                ERR_PRINT("shm recv worker got nullptr instead of shm recv block, worker exits");
                return;
            }

            uint32_t wait_err_count = 0;
            while (!stop_requested() && !stop_runing) {
                auto werr = shm->wait();
                if (werr != evt::NamedEventErr::None) {
                    wait_err_count += 1;
                    if (wait_err_count > 10) {
                        ERR_PRINT("recv worker wait() error, worker exits");
                        evtlog::error(elog_kind::ShmRecvWorker_Error, elog_cat::Worker);
                        break;
                    }
                    continue;
                }
                wait_err_count = 0;

                evtlog::info(elog_kind::ShmRecvWorker_Start, elog_cat::Worker);
                if (stop_requested()) break;

                // consume data until no records
                bool data_available = true;
                while (data_available) {
                    shm::ShmRecvPayload recv;
                    auto rerr = shm->read_data(recv);

                    switch (rerr) {
                        case shm::ShmRecvErr::None: {
                            m_timeout.stop();
                            break; 
                        }
                        
                        case shm::ShmRecvErr::NoRecords: { 
                            data_available = false; 
                            m_timeout.stop();
                            continue; 
                        }

                        // if we run into a NotYetPublished for long enough
                        // a writer must have died at exactly the wrong moment
                        // flush the messages and continue
                        case shm::ShmRecvErr::NotYetPublished: { 
                            // TODO: add a back-off strategy
                            // if this keeps happening, re-init the block
                            m_timeout.start(); // if already started this is no-op
                            if (m_timeout.duration() > MAX_TIMEOUT_MS) {
                                m_timeout.stop();
                                ERR_PRINT("shm recv worker flushing backlog due to publisher timeout");
                                shm->flush_backlog();
                                data_available = false;
                            }
                            std::this_thread::yield(); // allow someone else to run
                            continue;
                         }

                        case shm::ShmRecvErr::BlockNotInitialized: { 
                            ERR_PRINT("shm recv worker block not initialized, worker exits");
                            evtlog::error(elog_kind::ShmRecvWorker_Error, elog_cat::Worker);
                            stop_runing = true;
                            m_timeout.stop();
                            break;
                        }

                        case shm::ShmRecvErr::TailCorruption:   // fall through
                        case shm::ShmRecvErr::BlockCorrupted:   // fall through
                        case shm::ShmRecvErr::UnknownError: { 
                            ERR_PRINT("shm recv worker re-initializing shared memory block due to correction, err=", static_cast<int>(rerr));
                            shm->reinit();
                            m_timeout.stop();
                            data_available = false; 
                            continue;
                        }

                        default: {
                            ERR_PRINT("recv worker got unhandled error case");
                            data_available = false; 
                            m_timeout.stop();
                            continue;
                        }
                    }
                    
                    // move ptr passed header to actual data for distribution
                    auto* hdr = reinterpret_cast<io::LabelHeader*>(recv.buf.get());
                    const Label label = hdr->label;
                    const size_t data_size = static_cast<size_t>(hdr->data_size);
                    const size_t recv_offset = static_cast<size_t>(hdr->recv_offset);
                    auto* data_ptr = recv.buf.get() + sizeof(io::LabelHeader);
                    m_router.recv_from_publisher(label, data_ptr, data_size, recv_offset);
                }
                evtlog::info(elog_kind::ShmRecvWorker_End, elog_cat::Worker);
            }
        } catch (const std::exception& e) {
            ERR_PRINT("shm recv worker got exception, worker stopping: ", e.what());
            // TODO: this thread is exiting, maybe because the wait event was removed
        } catch (...) {
            ERR_PRINT("shm recv worker got unknown exception, worker stopping");
            // TODO: this thread is exiting, maybe because the wait event was removed
        }
        
        evtlog::info(elog_kind::ShmRecvWorker_Exit, elog_cat::Worker);
    }

    void ShmRecvWorker::on_stop_requested() {
        // TODO: we should never request stop on this worker
        // but if we did, how would we stop them?
        // we cannot POST to wake up the worker
        // and closing the block doesnt guarantee the worker will wake
        //
        // if (ev != nullptr) {
        //     // wake worker so they see the stop request
        //     ev->post();
        // }
    }
}