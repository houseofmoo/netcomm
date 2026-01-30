#include "router.h"

#include <cstring>
#include <mutex>
#include <eROIL/print.h>
#include <algorithm>
#include "comm/write_iosb.h"

namespace eroil {
    // open/close send/recv
    void Router::register_send_publisher(std::shared_ptr<hndl::SendHandle> handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        const handle_uid uid = handle->uid;
        const Label label = handle->data.label;

        // store handle
        auto [_, inserted] = m_send_handles.try_emplace(uid, std::move(handle));
        if (!inserted) {
            ERR_PRINT("duplicate send handle uid=", uid);
            return;
        }

        // add handle to route
        hndl::SendHandle* ptr = m_send_handles[uid].get();
        if (!m_routes.add_send_publisher(label, ptr)) {
            ERR_PRINT("failed to add handle to send route=", label);
            m_send_handles.erase(uid);
        }
    }

    void Router::unregister_send_publisher(const hndl::SendHandle* handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        // validate pointer still matches uid
        auto it = m_send_handles.find(handle->uid);
        if (it == m_send_handles.end() || it->second.get() != handle) {
            ERR_PRINT("invalid SendHandle*");
            return;
        }

        const Label label = handle->data.label;
        const handle_uid uid = handle->uid;

        // erase handle first
        m_send_handles.erase(it);

        // remove from route table (removes uid from publishers; may delete route if unused)
        if (!m_routes.remove_send_publisher(label, uid)) {
            ERR_PRINT("route table did not contain publisher uid=", uid, " label=", label);
            // continue anyway
        }
    }

    void Router::register_recv_subscriber(std::shared_ptr<hndl::RecvHandle> handle) {
        if (handle == nullptr) {
            ERR_PRINT("invalid handle");
            return;
        }

        std::unique_lock lock(m_router_mtx);

        const handle_uid uid = handle->uid;
        const Label label = handle->data.label;

        auto [_, h_inserted] = m_recv_handles.try_emplace(uid, std::move(handle));
        if (!h_inserted) {
            ERR_PRINT("duplicate recv handle uid=", uid);
            return;
        }

        // update routes
        hndl::RecvHandle* ptr = m_recv_handles[uid].get();
        if (!m_routes.add_recv_subscriber(label, ptr)) {
            ERR_PRINT("failed to add handle to recv route=", label);
            m_recv_handles.erase(uid);
        }
    }

    void Router::unregister_recv_subscriber(const hndl::RecvHandle* handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        auto it = m_recv_handles.find(handle->uid);
        if (it == m_recv_handles.end() || it->second.get() != handle) {
            ERR_PRINT("invalid RecvHandle*");
            return;
        }

        const handle_uid uid = handle->uid;
        const Label label = handle->data.label;

        m_recv_handles.erase(uid);

        if (!m_routes.remove_recv_subscriber(label, uid)) {
            ERR_PRINT("route table did not contain subscriber uid=", uid, " label=", label);
        }
    }

    hndl::SendHandle* Router::get_send_handle(handle_uid uid) {
        std::shared_lock lock(m_router_mtx);
        auto it = m_send_handles.find(uid);
        if (it == m_send_handles.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    hndl::RecvHandle* Router::get_recv_handle(handle_uid uid) {
        std::shared_lock lock(m_router_mtx);
        auto it = m_recv_handles.find(uid);
        if (it == m_recv_handles.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    // route interface
    void Router::add_local_send_subscriber(Label label, size_t size, NodeId dst_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.has_send_shm(dst_id)) {
            ERR_PRINT(": missing shm to nodeid=", dst_id);
            return;
        }

        if (!m_routes.add_local_send_subscriber(label, size, dst_id)) {
            ERR_PRINT(": failed to add local send subscriber for label=", label, " to_id=", dst_id);
            return;
        }
    }

    void Router::add_remote_send_subscriber(Label label, size_t label_size, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.has_socket(to_id)) {
            ERR_PRINT(": missing socket to NodeId=", to_id);
            return;
        }

        if (!m_routes.add_remote_send_subscriber(label, label_size, to_id)) {
            ERR_PRINT(": failed to add remote send subscriber for label=", label, " to_id=", to_id);
            return;
        }
    }

    void Router::remove_local_send_subscriber(Label label, NodeId dst_id) {
        std::unique_lock lock(m_router_mtx);
        m_routes.remove_local_send_subscriber(label, dst_id);
    }

    void Router::remove_remote_send_subscriber(Label label, NodeId dst_id) {
        std::unique_lock lock(m_router_mtx);
        m_routes.remove_remote_send_subscriber(label, dst_id);
    }

    io::LabelsSnapshot Router::get_send_labels_snapshot() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_send_labels_snapshot();
    }

    io::LabelsSnapshot Router::get_recv_labels_snapshot() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_recv_labels_snapshot();
    }

    std::array<io::LabelInfo, MAX_LABELS> Router::get_send_labels() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_send_labels();
    }

    bool Router::has_send_route(Label label) const noexcept {
        std::shared_lock lock(m_router_mtx);
        return m_routes.has_send_route(label);
    }

    bool Router::has_recv_route(Label label) const noexcept {
        std::shared_lock lock(m_router_mtx);
        return m_routes.has_recv_route(label);
    }

    bool Router::is_send_subscriber(Label label, NodeId to_id) const noexcept {
        std::shared_lock lock(m_router_mtx);
        if (m_routes.is_remote_send_subscriber(label, to_id)) {
            return true;
        }
        return m_routes.is_local_send_subscriber(label, to_id);
    }

    bool Router::upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock) {
        std::unique_lock lock(m_router_mtx);
        return m_transports.upsert_socket(id, sock);
    }

    std::shared_ptr<sock::TCPClient> Router::get_socket(NodeId id) const noexcept {
        std::unique_lock lock(m_router_mtx);
        return m_transports.get_socket(id);
    }

    bool Router::has_socket(NodeId id) const noexcept {
        std::shared_lock lock(m_router_mtx);
        return m_transports.has_socket(id);
    }

    bool Router::open_send_shm(NodeId dst_id) {
        std::unique_lock lock(m_router_mtx);
        return m_transports.open_send_shm(dst_id);
    }

    std::shared_ptr<shm::ShmSend> Router::get_send_shm(NodeId dst_id) const noexcept {
        std::shared_lock lock(m_router_mtx);
        return m_transports.get_send_shm(dst_id);
    }

    bool Router::open_recv_shm(NodeId my_id) {
        std::unique_lock lock(m_router_mtx);
        return m_transports.open_recv_shm(my_id);
    }

    std::shared_ptr<shm::ShmRecv> Router::get_recv_shm() const noexcept {
        std::shared_lock lock(m_router_mtx);
        return m_transports.get_recv_shm();
    }

    std::pair<io::SendJobErr, std::shared_ptr<io::SendJob>> 
    Router::build_send_job(const NodeId my_id, const Label label, const handle_uid uid, io::SendBuf send_buf) {
        static uint32_t seq = 0;
        auto job = std::make_shared<io::SendJob>(std::move(send_buf));
        job->source_id = my_id;
        job->label = label;
        job->seq = seq++;

        {
            std::shared_lock lock(m_router_mtx);
            DB_ASSERT(job->send_buffer.total_size <= MAX_LABEL_SEND_SIZE, "label too large to send");

            const SendRoute* route = m_routes.get_send_route(label);
            if (route == nullptr) {
                ERR_PRINT("no route for label=", label);
                return { io::SendJobErr::RouteNotFound, job };
            }

            size_t expected_size = route->label_size + sizeof(io::LabelHeader);
            if (expected_size != job->send_buffer.total_size ) {
                ERR_PRINT("size mismatch label=", label, " expected=", expected_size, " got=", send_buf.total_size);
                return { io::SendJobErr::SizeMismatch, job };
            }

            auto handle_it = m_send_handles.find(uid);
            if (handle_it == m_send_handles.end()) {
                ERR_PRINT("got handle uid that does not match any known send handles, uid=", uid);
                return { io::SendJobErr::UnknownHandle, job };
            }

            std::vector<handle_uid> uids = m_routes.snapshot_send_publishers(label);
            if (uids.empty()) {
                ERR_PRINT("no send publishers for label=", label);
                return { io::SendJobErr::NoPublishers, job };
            }

            // confirm this uid is a publisher
            auto uids_it = std::find(
                uids.begin(),
                uids.end(),
                uid
            );
            if (uids_it == uids.end()) {
                ERR_PRINT("handle was not a member of the send publishers list, uid=", uid);
                return { io::SendJobErr::IncorrectPublisher, job };
            }

            // store publisher
            job->publisher = handle_it->second;

            // snapshot local subs
            if (!route->local_subscribers.empty()) {
                job->local_recvrs.reserve(route->local_subscribers.size());
                for (const NodeId local : route->local_subscribers) {
                    job->local_recvrs.push_back(
                        m_transports.get_send_shm(local)
                    );
                }
            }
            
            // snapshot remote subs
            if (!route->remote_subscribers.empty()) {
                job->remote_recvrs.reserve(route->remote_subscribers.size());
                for (const NodeId remote : route->remote_subscribers) {
                    job->remote_recvrs.push_back(
                        m_transports.get_socket(remote)
                    );
                }
            }
        }

        job->pending_sends.store(job->local_recvrs.size() + job->remote_recvrs.size(), std::memory_order_relaxed);
        return { io::SendJobErr::None, job };
    }

    void Router::distribute_recvd_label(const NodeId source_id, 
                                        const Label label, 
                                        const std::byte* buf, 
                                        const size_t size, 
                                        const size_t recv_offset) const {
        if (buf == nullptr || size == 0) return;
        
        // get subscribers snapshot
        std::vector<std::shared_ptr<hndl::RecvHandle>> subscribers;
        {
            std::shared_lock lock(m_router_mtx);
            
            const RecvRoute* route = m_routes.get_recv_route(label);
            if (route == nullptr) {
                ERR_PRINT("unknown label=", label);
                return;
            }

            if (route->label_size != size) {
                ERR_PRINT("size mismatch label=", label,
                          " expected=", route->label_size, " got=", size);
                return;
            }

            std::vector<handle_uid> uids = m_routes.snapshot_recv_subscribers(label);
            if (uids.empty()) {
                ERR_PRINT("no recv subscribers for label=", label);
                return;
            }

            subscribers.reserve(uids.size());
            for (handle_uid uid : uids) {
                auto it = m_recv_handles.find(uid);
                if (it != m_recv_handles.end() && it->second) {
                    subscribers.push_back(it->second);
                }
            }
        }

        // write to subscribers buffers
        for (const auto& sub : subscribers) {
            if (sub == nullptr) {
                ERR_PRINT("got null subscriber for label=", label);
                continue;
            }

            if (sub->data.buf == nullptr) { 
                ERR_PRINT("subscriber has no buffer for label=", label);
                continue;
            }

            if (sub->data.buf_slots == 0) {
                ERR_PRINT("subscriber has no buffer slots for label=", label);
                continue;
            }

            if (sub->data.buf_size == 0) { 
                ERR_PRINT("subscriber buffer size is 0 for label=", label);
                continue;
            }

            if (recv_offset > sub->data.buf_size) {
                ERR_PRINT("recv offset > buf size for label=", label);
                continue;
            }

            // TODO: right now we always replace the oldeest buffer slot which makes sense
            // to me, but im unclear if that is the way it works in NAE

            std::lock_guard lock(sub->mtx);
            switch (sub->data.signal_mode) {
                // signal on error or buffer full, not allowed to overwrite
                case iosb::SignalMode::BUFFER_FULL: {
                    // full and not allowed to overwrite, signal again
                    if (sub->data.recv_count == sub->data.buf_slots) {
                        // TODO: actually this is an error, do we write an IOSB for the error?
                        // comm::write_recv_iosb(sub.get(), source_id, label, size, recv_offset, dst);
                        plat::try_signal_sem(sub->data.sem);
                    } else {
                        // not full yet, copy data into next buffer slot
                        const size_t slot = sub->data.buf_index % sub->data.buf_slots;
                        std::byte* dst = sub->data.buf + (slot * sub->data.buf_size);
                        std::memcpy(dst + recv_offset, buf, size);
                        
                        sub->data.recv_count += 1;
                        sub->data.buf_index = (sub->data.buf_index + 1) % sub->data.buf_slots;

                        // TODO: do we only write IOSB when full or do we write IOSB every message
                        // but only signal on full?

                        // if we are now full, signal
                        if (sub->data.recv_count == sub->data.buf_slots) {
                            comm::write_recv_iosb(sub.get(), source_id, label, size, recv_offset, dst);
                            plat::try_signal_sem(sub->data.sem);
                        }
                    }
                    break;
                }

                // signal on error only, allowed to overwrite
                case iosb::SignalMode::OVERWRITE: {
                    const size_t slot = sub->data.buf_index % sub->data.buf_slots;
                    std::byte* dst = sub->data.buf + (slot * sub->data.buf_size);
                    std::memcpy(dst + recv_offset, buf, size);
                    
                    sub->data.recv_count += 1;
                    sub->data.buf_index = (sub->data.buf_index + 1) % sub->data.buf_slots;

                    // TODO: actually im not sure if we write IOSB and don't signal the sem
                    // or we dont write the IOSB at all
                    comm::write_recv_iosb(sub.get(), source_id, label, size, recv_offset, dst);
                    break;
                }
                
                // signal every message, not allowed to overwrite
                case iosb::SignalMode::EVERY_MESSAGE: {
                    std::byte* dst = sub->data.buf + (sub->data.buf_index * sub->data.buf_size);
                    if (sub->data.recv_count < sub->data.buf_slots) {
                        std::memcpy(dst + recv_offset, buf, size);
                    
                        sub->data.recv_count += 1;
                        sub->data.buf_index = (sub->data.buf_index + 1) % sub->data.buf_slots;
                    }

                    comm::write_recv_iosb(sub.get(), source_id, label, size, recv_offset, dst);
                    plat::try_signal_sem(sub->data.sem);
                    break;
                }

                default: {
                    ERR_PRINT("got unknown signal mode=", static_cast<int32_t>(sub->data.signal_mode));
                    break;
                }
            }
        }
    }
}
