#include "shm_recv_worker.h"
#include <eROIL/print.h>
#include "types/const_types.h"
#include "log/evtlog_api.h"
#include "timer/timer.h"

namespace eroil::worker {
    ShmRecvWorker::ShmRecvWorker(Router& router, NodeId id) : 
        m_router{router}, m_id{id}, m_shm{nullptr} {
    }

    void ShmRecvWorker::start() {
        m_shm = m_router.get_recv_shm();
        if (m_shm == nullptr) {
            ERR_PRINT("shm recv worker got nullptr instead of shm recv block, worker exits");
            return;
        }

        if (m_thread.joinable()) return;
        m_stop.store(false, std::memory_order_release);
        m_thread = std::thread([this] { run(); });
    }

    void ShmRecvWorker::stop() {
        //TODO: if someone external calls stop, we will wait on join() until someone wakes us up
        // in theory this doesnt matter cause this thread should live the entire time the application does
        //bool was_stopping = m_stop.exchange(true, std::memory_order_acq_rel);
        m_stop.exchange(true, std::memory_order_acq_rel);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void ShmRecvWorker::run() {
        try {
            uint32_t wait_err_count = 0;
            while (!stop_requested()) {
                auto werr = m_shm->wait();
                if (werr != evt::NamedEventErr::None) {
                    wait_err_count += 1;
                    if (wait_err_count > 10) {
                        ERR_PRINT("recv worker wait() error, worker exits");
                        evtlog::error(elog_kind::WaitError, elog_cat::ShmRecvWorker);
                        break;
                    }
                    continue;
                }
                wait_err_count = 0;

                if (stop_requested()) break;
                EvtMark mark(elog_cat::ShmRecvWorker);

                // consume data until no records
                while (true) {
                    auto [has_data, record] = get_next_record();
                    if (!has_data) break;

                    // validation on recv'd data header
                    if (record.buf_size < sizeof(io::LabelHeader)) {
                        ERR_PRINT("shm recv got a record that was too small to contain a label header");
                        evtlog::error(elog_kind::MalformedRecv, elog_cat::ShmRecvWorker);
                        continue;
                    }

                    auto* hdr = reinterpret_cast<io::LabelHeader*>(record.buf.get());
                    if (hdr->magic != MAGIC_NUM || hdr->version != VERSION) {
                        ERR_PRINT("shm recv got a header that did not have the correct magic and/or version");
                        evtlog::error(elog_kind::InvalidHeader, elog_cat::ShmRecvWorker);
                        continue;
                    }

                    if (hdr->data_size > MAX_LABEL_SEND_SIZE) {
                        ERR_PRINT("shm recv got header that indicates data size is > ", MAX_LABEL_SEND_SIZE);
                        ERR_PRINT("    label=", hdr->label, ", sourceid=", hdr->source_id);
                        evtlog::error(elog_kind::InvalidDataSize, elog_cat::ShmRecvWorker, hdr->label, hdr->data_size);
                        break;
                    }

                    m_router.distribute_recvd_label(
                        static_cast<Label>(hdr->label),
                        record.buf.get() + sizeof(io::LabelHeader),
                        static_cast<size_t>(hdr->data_size),
                        static_cast<size_t>(hdr->recv_offset)
                    );
                    evtlog::info(elog_kind::DataDistributed, elog_cat::ShmRecvWorker);
                }
            }
        } catch (const std::exception& e) {
            ERR_PRINT("shm recv worker got exception, worker stopping: ", e.what());
        } catch (...) {
            ERR_PRINT("shm recv worker got unknown exception, worker stopping");
        }
        
        evtlog::info(elog_kind::Exit, elog_cat::ShmRecvWorker);
    }

    std::pair<bool, shm::ShmRecvResult> ShmRecvWorker::get_next_record() {
        time::Timer timer;
        while (true) {
            auto result = m_shm->recv();

            switch (result.err) {
                case shm::ShmRecvErr::None: {
                    return { true, std::move(result) };
                }
                
                case shm::ShmRecvErr::NoRecords: { 
                    return { false, std::move(result) };
                }

                // next record isnt published, continue trying until we get it or timer expires
                case shm::ShmRecvErr::NotYetPublished: { 
                    timer.start(); // if already started this is no-op
                    if (timer.duration() > MAX_TIMEOUT_MS) {
                        ERR_PRINT("shm recv worker flushing backlog due to publisher timeout");
                        m_shm->flush_backlog();
                        evtlog::warn(elog_kind::PublishTimeout, elog_cat::ShmRecvWorker);
                        return { false, std::move(result) };
                    }
                    std::this_thread::yield();
                    continue;
                }

                case shm::ShmRecvErr::BlockNotInitialized: { 
                    ERR_PRINT("shm recv worker block not initialized, worker exits");
                    m_stop.store(true, std::memory_order_release);
                    evtlog::error(elog_kind::BlockNotInitialized, elog_cat::ShmRecvWorker);
                    return { false, std::move(result) };
                }

                case shm::ShmRecvErr::TailCorruption: // fall through
                case shm::ShmRecvErr::BlockCorrupted: {
                    ERR_PRINT("shm recv worker re-initializing shared memory block due to corruption, err=", static_cast<int>(result.err));
                    m_shm->reinit();
                    evtlog::error(elog_kind::BlockCorruption, elog_cat::ShmRecvWorker);
                    return { false, std::move(result) };
                }
            
                case shm::ShmRecvErr::UnknownError: { 
                    ERR_PRINT("shm recv worker re-initializing due to unknown error");
                    m_shm->reinit();
                    evtlog::error(elog_kind::UnknownError, elog_cat::ShmRecvWorker);
                    return { false, std::move(result) };
                }

                default: {
                    ERR_PRINT("recv worker got unhandled error case");
                    evtlog::error(elog_kind::UnhandledError, elog_cat::ShmRecvWorker);
                    return { false, std::move(result) };
                }
            }
        }
    }
}