#include "shm_recv_worker.h"
#include <eROIL/print.h>

namespace eroil::worker {
    ShmRecvWorker::ShmRecvWorker(Router& router, 
                                 Label label, 
                                 size_t label_size, 
                                 std::shared_ptr<shm::ShmRecv> recv)
        : m_router(router), m_label(label), m_label_size(label_size), m_recv_shm(std::move(recv)) {}

    void ShmRecvWorker::launch() { 
        start(); 
    }

    void ShmRecvWorker::request_unblock() {
        if (m_recv_shm) {
            // attempt to wake sleeping thread so they can exit
            m_recv_shm->interrupt_wait();
        }
    }

    void ShmRecvWorker::run() {
        try {
            std::vector<std::byte> tmp;
            tmp.resize(m_label_size);
            while (!stop_requested()) {
                //NodeId from_id{};
                auto err = m_recv_shm->recv(tmp.data(), m_label_size); // blocks
                if (stop_requested()) break;

                if (err != shm::ShmOpErr::None) {
                    // either interrupted or underlying error
                    ERR_PRINT("shm recv worker got error waiting for recv for label=", m_label);
                    continue;
                }

                // TODO: maybe add from_id to the recv from publisher
                m_router.recv_from_publisher(m_label, tmp.data(), tmp.size());
            }
        } catch (const std::exception& e) {
            ERR_PRINT("shm recv worker got exception, worker stopping: ", e.what());
            // TODO: this thread is exiting, but i assume the SHM block is still OK
            // unless an exception occured due to the shm block. who the fuck knows, i just
            // know we're all fucked up
        } catch (...) {
            ERR_PRINT("shm recv worker for unknown exception, worker stopping");
            // TODO: this thread is exiting, but i assume the SHM block is still OK
            // unless an exception occured due to the shm block. who the fuck knows, i just
            // know we're all fucked up
        }
    }
}


