#include "socket_recv_worker.h"
#include <eROIL/print.h>
#include "types/const_types.h"
#include "address/address.h"
#include "log/evtlog_api.h"

namespace eroil::wrk {
    SocketRecvWorker::SocketRecvWorker(rt::Router& router, NodeId id, NodeId peer_id) : 
        m_router(router), m_id(id), m_peer_id(peer_id), m_sock(nullptr) {
            m_sock = m_router.get_socket(m_peer_id);
        }

    void SocketRecvWorker::start() {
        if (m_thread.joinable()) {
            ERR_PRINT("attempted to start a joinable thread (double start or start after stop but before join() called)");
            return;
        }
        m_stop.store(false, std::memory_order_release);
        m_thread = std::thread([this] { run(); });
    }

    void SocketRecvWorker::stop() {
        // NOTE: socket recv workers can be stopped in two cases:
        //  1) monitor thread sees a dead socket, it will stop the socket recv worker
        //  2) socket recv worker sees incoherent header, will kill the socket and exit
        // in either case the monitor thread will see the dead socket and attempt reconnection.
        // upon reconnection the socket recv worker thread will be re-launched
        m_stop.exchange(true, std::memory_order_acq_rel);
        if (m_thread.joinable()) {
            // do not allow this thread to call join on itself
            if (std::this_thread::get_id() != m_thread.get_id()) {
                m_thread.join();
            }
        }
    }

    void SocketRecvWorker::run() {
        try {
            // set up a temp buffer for label data that can hold max label size
            // we dont resize the buff to max label size because with sockets we get to see
            // the header before the payload and can resize as required then
            std::vector<std::byte> payload;
            payload.reserve(MAX_LABEL_SIZE);
            io::LabelHeader hdr{};

            while (!stop_requested()) {
                if (!m_sock->is_connected()) {
                    ERR_PRINT("socket recv worker socket disconnected, worker exits");
                    break;
                }
                
                if (!recv_exact(reinterpret_cast<std::byte*>(&hdr), sizeof(hdr))) {
                    ERR_PRINT("socket recv failed to get expected header size, worker exits");
                    break;
                }
                
                if (stop_requested()) {
                    LOG("socket recv worker got stop request, worker exits");
                    break;
                }

                EvtMark mark(elog_cat::SocketRecvWorker);

                if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                    // we cannot figure out how to drain this socket, disconnect it and monitor thread will re-establish comms'/
                    evtlog::error(elog_kind::InvalidHeader, elog_cat::SocketRecvWorker);
                    ERR_PRINT("socket recv got a header that did not have the correct magic and/or version");
                    disconnect_and_stop();
                    break;
                }
                
                if (hdr.data_size > MAX_LABEL_SIZE) {
                    ERR_PRINT("socket recv got header that indicates data size is > ", MAX_LABEL_SIZE);
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id);
                    evtlog::error(elog_kind::InvalidDataSize, elog_cat::SocketRecvWorker, hdr.label, hdr.data_size);
                    disconnect_and_stop();
                    break;
                }
                
                if (hdr.data_size <= 0 && io::has_flag(hdr.flags, io::LabelFlag::Data)) {
                    ERR_PRINT("socket recv got header that indicates data size is 0");
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id);
                    evtlog::error(elog_kind::InvalidDataSize, elog_cat::SocketRecvWorker, hdr.label, hdr.data_size);
                    disconnect_and_stop();
                    break;
                }

                // not data, check if ping
                if (!io::has_flag(hdr.flags, io::LabelFlag::Data)) {
                    // was a ping, continue normally -> pings do not contain data
                    if (io::has_flag(hdr.flags, io::LabelFlag::Ping)) {
                        continue;
                    }

                    // not a ping, something is wrong
                    ERR_PRINT("socket recv got header that indicates neither ping or data");
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id, " flags=", hdr.flags);
                    evtlog::error(elog_kind::InvalidFlags, elog_cat::Worker, hdr.label, hdr.flags);
                    disconnect_and_stop();
                    break;
                }

                payload.resize(hdr.data_size);
                if (!recv_exact(payload.data(), payload.size())) {
                    ERR_PRINT("socket recv failed to get expected data size, size=", hdr.data_size);
                    ERR_PRINT("    label=", hdr.label, ", sourceid=", hdr.source_id);
                    evtlog::error(elog_kind::RecvError, elog_cat::SocketRecvWorker);
                    break;
                }
                if (stop_requested()) break;

                m_router.distribute_recvd_label(
                    static_cast<NodeId>(hdr.source_id),
                    static_cast<Label>(hdr.label), 
                    payload.data(),
                    payload.size(), 
                    static_cast<size_t>(hdr.recv_offset)
                );
            }
        } catch (const std::exception& e) {
            ERR_PRINT("socket recv exception: ", e.what());
        } catch (...) {
            ERR_PRINT("unknown socket recv exception");
        }

        evtlog::info(elog_kind::Exit, elog_cat::SocketRecvWorker, m_peer_id);
        PRINT("socket recv worker for nodeid=", m_peer_id, " exits");
    }

    bool SocketRecvWorker::recv_exact(std::byte* dst, const size_t size) {
        if (stop_requested()) return false;
        sock::SockResult result = m_sock->recv_all(dst, size);
        switch (result.code) {
            case sock::SockErr::None: {
                if (result.bytes <= 0) return false; // should never happen
                break;
            }

            case sock::SockErr::WouldBlock: {
                // should never happen with blocking sockets
                std::this_thread::yield();
                break;
            }

            case sock::SockErr::Closed:     // fallthrough
            case sock::SockErr::Unknown:    // fallthrough
            default: return false;
        }
        return true;
    }

    void SocketRecvWorker::disconnect_and_stop() {
        if (!m_stop.exchange(true, std::memory_order_acq_rel)) {
            if (m_sock != nullptr) { m_sock->disconnect(); }
        }
    }
}

