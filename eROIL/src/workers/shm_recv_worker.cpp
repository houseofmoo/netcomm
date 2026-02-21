#include "shm_recv_worker.h"
#include <eROIL/print.h>
#include "types/const_types.h"
#include "log/evtlog_api.h"
#include "time/timer.h"

namespace eroil::wrk {
    ShmRecvWorker::ShmRecvWorker(rt::Router& router, NodeId id) : 
        m_router{router}, m_id{id}, m_shm{nullptr} {
    }

    void ShmRecvWorker::start() {
        m_shm = m_router.get_recv_shm();
        if (m_shm == nullptr) {
            ERR_PRINT("shm recv worker got nullptr instead of shm recv block, worker exits");
            return;
        }

        if (m_thread.joinable()){
            ERR_PRINT("attempted to start a joinable thread (double start or start after stop but before join() called)");
            return;
        }
        m_stop.store(false, std::memory_order_release);
        m_thread = std::thread([this] { run(); });
    }

    void ShmRecvWorker::stop() {
        // NOTE: shm recv worker stopping is not expected behavior.
        // this thread should live for the lifetime of the application.
        // if somewhere down the life someone changes things and decides we need to stop
        // this worker, we will wait on join() until someone wakes us up which may be never
        
        m_stop.exchange(true, std::memory_order_acq_rel);
        if (m_thread.joinable()) {
            // do not allow this thread to call join on itself
            if (std::this_thread::get_id() != m_thread.get_id()) {
                m_thread.join();
            }
        }
    }

    void ShmRecvWorker::run() {
        try {
            // set up a temp buffer that can hold label header + max label size
            // set recv buffer size to max possible size since we recv the label header
            // and label data in one recv call and we want to always ensure we have enough
            // space for the entire record in the buffer to avoid partial recv issues
            std::vector<std::byte> recv_buf;
            recv_buf.resize(MAX_LABEL_SIZE + sizeof(io::LabelHeader));
            uint32_t wait_err_count = 0;

            while (!stop_requested()) {
                evt::NamedSemResult werr = m_shm->wait();
                if (!werr.ok()) {
                    wait_err_count += 1;
                    if (wait_err_count > 10) {
                        ERR_PRINT("recv worker wait() error 10 consecutive times, worker exits, err=", werr.code_to_string());
                        evtlog::error(elog_kind::WaitError, elog_cat::ShmRecvWorker);
                        break;
                    }
                    continue;
                }
                wait_err_count = 0;

                if (stop_requested()) {
                    LOG("shm recv worker got stop request, worker exits");
                    break;
                }

                EvtMark mark(elog_cat::ShmRecvWorker);

                // consume data until no records
                while (true) {
                    auto [has_data, record] = get_next_record(recv_buf.data(), recv_buf.size());
                    if (!has_data) break;

                    if (record.buf_size < sizeof(io::LabelHeader)) {
                        ERR_PRINT("shm recv got a record that was too small to contain a label header");
                        evtlog::error(elog_kind::MalformedRecv, elog_cat::ShmRecvWorker);
                        continue;
                    }

                    auto* hdr = reinterpret_cast<io::LabelHeader*>(record.recv_buf);
                    if (hdr->magic != MAGIC_NUM || hdr->version != VERSION) {
                        ERR_PRINT("shm recv got a header that did not have the correct magic and/or version");
                        evtlog::error(elog_kind::InvalidHeader, elog_cat::ShmRecvWorker);
                        continue;
                    }

                    if (hdr->label_size > MAX_LABEL_SIZE) {
                        ERR_PRINT("shm recv got header that indicates label size is > ", MAX_LABEL_SIZE);
                        ERR_PRINT("    label=", hdr->label, ", sourceid=", hdr->source_id);
                        evtlog::error(elog_kind::InvalidLabelSize, elog_cat::ShmRecvWorker, hdr->label, hdr->label_size);
                        break;
                    }

                    auto* data_ptr = record.recv_buf + sizeof(io::LabelHeader);
                    m_router.distribute_recvd_label(
                        static_cast<NodeId>(hdr->source_id),
                        static_cast<Label>(hdr->label),
                        data_ptr,
                        static_cast<size_t>(hdr->label_size),
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

    std::pair<bool, shm::ShmRecvData> ShmRecvWorker::get_next_record(std::byte* recv_buf, const size_t recv_buf_size) {
        time::Timer timer;
        while (true) {
            shm::ShmRecvData data = m_shm->recv(recv_buf, recv_buf_size);

            switch (data.result.code) {
                case shm::ShmRecvErr::None: {
                    return { true, data };
                }
                
                case shm::ShmRecvErr::NoRecords: { 
                    return { false, data };
                }

                // next record isnt published, continue trying until we get it or timer expires
                case shm::ShmRecvErr::NotYetPublished: { 
                    timer.start(); // if already started this is no-op
                    if (timer.duration() > MAX_TIMEOUT_MS) {
                        ERR_PRINT("shm recv worker flushing backlog due to publisher timeout");
                        m_shm->flush_backlog();
                        evtlog::warn(elog_kind::PublishTimeout, elog_cat::ShmRecvWorker);
                        return { false, data };
                    }
                    std::this_thread::yield();
                    continue;
                }

                case shm::ShmRecvErr::BlockNotInitialized: { 
                    ERR_PRINT("shm recv worker block not initialized, worker exits");
                    m_stop.store(true, std::memory_order_release);
                    evtlog::error(elog_kind::BlockNotInitialized, elog_cat::ShmRecvWorker);
                    return { false, data };
                }

                case shm::ShmRecvErr::TailCorruption: // fall through
                case shm::ShmRecvErr::BlockCorrupted: {
                    ERR_PRINT("shm recv worker re-initializing shared memory block due to corruption, err=", data.result.code_to_string());
                    m_shm->reinit();
                    evtlog::error(elog_kind::BlockCorruption, elog_cat::ShmRecvWorker);
                    return { false, data };
                }

                case shm::ShmRecvErr::LabelTooLarge: {
                    ERR_PRINT("shm recv worker got a record with label size larger than recv buffer, record skipped");
                    evtlog::error(elog_kind::LabelTooLarge, elog_cat::ShmRecvWorker);
                    return { false, data };
                }
            
                case shm::ShmRecvErr::UnknownError: { 
                    ERR_PRINT("shm recv worker re-initializing shared memory block due to unknown error");
                    m_shm->reinit();
                    evtlog::error(elog_kind::UnknownError, elog_cat::ShmRecvWorker);
                    return { false, data };
                }

                default: {
                    ERR_PRINT("recv worker got unhandled error case");
                    evtlog::error(elog_kind::UnhandledError, elog_cat::ShmRecvWorker);
                    return { false, data };
                }
            }
        }
    }
}