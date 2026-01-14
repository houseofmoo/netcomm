#include "router.h"

#include <eROIL/print.h>
#include <algorithm>

namespace eroil {
    Router::Router() = default;
    Router::~Router() = default;

    // open/close send/recv
    void Router::register_send_publisher(std::unique_ptr<SendHandle> handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        const handle_uid uid = handle->uid;
        const Label label = handle->data->label;

        // store handle
        auto [_, inserted] = m_send_handles.try_emplace(uid, std::move(handle));
        if (!inserted) {
            ERR_PRINT("register_send_publisher(): duplicate send handle uid=", uid);
            return;
        }

        // add handle to route
        auto ptr = m_send_handles[uid].get();
        if (!m_routes.add_send_publisher(label, ptr)) {
            ERR_PRINT("register_send_publisher(): failed to add handle to send route=", label);
            m_send_handles.erase(uid);
        }
    }

    void Router::unregister_send_publisher(SendHandle* handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        // validate pointer still matches uid
        auto it = m_send_handles.find(handle->uid);
        if (it == m_send_handles.end() || it->second.get() != handle) {
            ERR_PRINT("unregister_send_publisher(): invalid SendHandle*");
            return;
        }

        const Label label = handle->data->label;
        const handle_uid uid = handle->uid;

        // erase handle first
        m_send_handles.erase(it);

        // remove from route table (removes uid from publishers; may delete route if unused)
        if (!m_routes.remove_send_publisher(label, uid)) {
            ERR_PRINT("unregister_send_publisher(): route table did not contain publisher uid=", uid, " label=", label);
            // continue anyway
        }

        // if route was deleted, close the shared memory block
        const auto route = m_routes.get_send_route(label);
        if (route == nullptr) {
            m_transports.delete_send_shm(label);
        }
    }

    void Router::register_recv_subscriber(std::unique_ptr<RecvHandle> handle) {
        if (handle == nullptr) {
            ERR_PRINT("invalid handle");
            return;
        }

        std::unique_lock lock(m_router_mtx);

        const handle_uid uid = handle->uid;
        const Label label = handle->data->label;

        auto [_, h_inserted] = m_recv_handles.try_emplace(uid, std::move(handle));
        if (!h_inserted) {
            ERR_PRINT("register_recv_subscriber(): duplicate recv handle uid=", uid);
            return;
        }

        // update routes
        auto ptr = m_recv_handles[uid].get();
        if (!m_routes.add_recv_subscriber(label, ptr)) {
            ERR_PRINT("register_recv_subscriber(): failed to add handle to recv route=", label);
            m_recv_handles.erase(uid);
        }
    }

    void Router::unregister_recv_subscriber(RecvHandle* handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        auto it = m_recv_handles.find(handle->uid);
        if (it == m_recv_handles.end() || it->second.get() != handle) {
            ERR_PRINT("unregister_recv_subscriber(): invalid RecvHandle*");
            return;
        }

        const handle_uid uid = handle->uid;
        const Label label = handle->data->label;

        m_recv_handles.erase(uid);

        if (!m_routes.remove_recv_subscriber(label, uid)) {
            ERR_PRINT("unregister_recv_subscriber(): route table did not contain subscriber uid=", uid, " label=", label);
        }

        // if route was deleted, close the shared memory block
        const auto route = m_routes.get_recv_route(label);
        if (route == nullptr) {
            m_transports.delete_recv_shm(label);
        }
    }

    // route interface
    void Router::add_local_send_subscriber(Label label, size_t label_size, NodeId my_id, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.open_send_shm(label, label_size)) {
            ERR_PRINT("add_local_send_subscriber(): could not create or open shm block for label=", label);
            return;
        }

        if (!m_routes.add_local_send_subscriber(label, label_size, my_id, to_id)) {
            ERR_PRINT("add_local_send_subscriber(): failed to add local send subscriber for label=", label, " to_id=", to_id);
            return;
        }
    }

    void Router::add_remote_send_subscriber(Label label, size_t label_size, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.has_socket(to_id)) {
            ERR_PRINT("add_remote_send_subscriber(): missing socket to NodeId=", to_id);
            return;
        }

        if (!m_routes.add_remote_send_subscriber(label, label_size, to_id)) {
            ERR_PRINT("add_remote_send_subscriber(): could not add remove send subscriber for label=", label, " to_id=", to_id);
            return;
        }
    }

    void Router::add_local_recv_publisher(Label label, size_t label_size, NodeId my_id, NodeId from_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.open_recv_shm(label, label_size)) {
            ERR_PRINT("add_local_recv_publisher(): could not create or open shm block for label=", label);
            return;
        }

        if (!m_routes.add_local_recv_publisher(label, label_size, my_id, from_id)) {
            ERR_PRINT("add_local_recv_publisher(): failed to add local recv publisher for label=", label, " from_id=", from_id);
            return;
        }
    }

    void Router::add_remote_recv_publisher(Label label, size_t size, NodeId from_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.has_socket(from_id)) {
            ERR_PRINT("add_remote_recv_publisher(): missing socket from NodeId=", from_id);
            return;
        }

        if (!m_routes.add_remote_recv_publisher(label, size, from_id)) {
            ERR_PRINT("add_remote_recv_publisher(): rejected (local publisher exists or overwrite) label=", label, " from_id=", from_id);
            return;
        }
    }

    void Router::remove_local_send_subscriber(Label label, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);
        m_routes.remove_local_send_subscriber(label, to_id);
    }

    void Router::remove_remote_send_subscriber(Label label, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);
        m_routes.remove_remote_send_subscriber(label, to_id);
    }

    void Router::remove_local_recv_publisher(Label label, NodeId from_id) {
        std::unique_lock lock(m_router_mtx);
        m_routes.remove_local_recv_publisher(label, from_id);
    }

    void Router::remove_remote_recv_publisher(Label label, NodeId from_id) {
        std::unique_lock lock(m_router_mtx);
        m_routes.remove_remote_recv_publisher(label, from_id);
    }

    std::array<LabelInfo, MAX_LABELS> Router::get_send_labels() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_send_labels();
    }

    std::array<LabelInfo, MAX_LABELS> Router::get_recv_labels() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_recv_labels();
    }

    bool Router::has_send_route(Label label) const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.has_send_route(label);
    }

    bool Router::has_recv_route(Label label) const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.has_recv_route(label);
    }

    bool Router::is_send_subscriber(Label label, NodeId to_id) const {
        std::shared_lock lock(m_router_mtx);
        if (m_routes.is_remote_send_subscriber(label, to_id)) {
            return true;
        }
        return m_routes.is_local_send_subscriber(label, to_id);
    }

    bool Router::is_recv_publisher(Label label, NodeId from_id) const {
        std::shared_lock lock(m_router_mtx);
        if (m_routes.is_remote_recv_publisher(label, from_id)) {
            return true;
        }
        return m_routes.is_local_recv_publisher(label, from_id);
    }

    bool Router::upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock) {
        return m_transports.upsert_socket(id, sock);
    }

    std::shared_ptr<sock::TCPClient> Router::get_socket(NodeId id) {
        return m_transports.get_socket(id);
    }

    std::vector<std::shared_ptr<sock::TCPClient>> Router::get_all_sockets() {
        return m_transports.get_all_sockets();
    }

    bool Router::has_socket(NodeId id) const {
        return m_transports.has_socket(id);
    }

    std::shared_ptr<shm::Shm> Router::get_send_shm(Label label) {
        return m_transports.get_send_shm(label);
    }

    std::shared_ptr<shm::Shm> Router::get_recv_shm(Label label) {
        return m_transports.get_recv_shm(label);
    }

    RecvRoute* Router::get_recv_route(Label label) {
        return m_routes.get_recv_route(label);
    }

    SendResult Router::send_to_subscribers(Label label, const void* buf, size_t size, handle_uid uid) {
        if (!buf || size == 0) return { SendOpErr::Failed, {}, {} };
        SendTargets targets{ {}, nullptr, {}, {} };
        {
            std::shared_lock lock(m_router_mtx);

            const SendRoute* route = m_routes.get_send_route(label);
            if (route == nullptr) {
                ERR_PRINT("send_to_subscribers(): unknown label=", label);
                return { SendOpErr::RouteNotFound, {}, {} };
            }

            size_t total_size = route->label_size + sizeof(LabelHeader);
            if (total_size != size ) {
                ERR_PRINT("send_to_subscribers(): size mismatch label=", label,
                          " expected=", total_size, " got=", size);
                return { SendOpErr::SizeMismatch, {}, {} };
            }

            if (size > SOCKET_DATA_MAX_SIZE) {
                ERR_PRINT("send_to_subscribers(): send size bigger than max label=", label,
                          " size=", size, " max=", SOCKET_DATA_MAX_SIZE);
                return { SendOpErr::SizeTooLarge, {}, {} };
            }

            auto handle_it = m_send_handles.find(uid);
            if (handle_it == m_send_handles.end()) {
                ERR_PRINT("send_to_subscribers(): got handle uid that does not match any known send handles, uid=", uid);
                return { SendOpErr::UnknownHandle, {}, {} };
            }

            auto uids = m_routes.snapshot_send_publishers(label);
            if (uids.empty()) {
                ERR_PRINT("send_to_subscribers(): no send publishers for label=", label);
                return { SendOpErr::NoPublishers, {}, {} };
            }

            // confirm this uid is a publisher
            auto uids_it = std::find(
                uids.begin(),
                uids.end(),
                uid
            );
            if (uids_it == uids.end()) {
                ERR_PRINT("send_to_subscribers(): handle was not a member of the send publishers list, uid=", uid);
                return { SendOpErr::IncorrectPublisher, {}, {} };
            }

            // store publisher
            targets.publisher = handle_it->second->data;

            // snapshot local subs
            if (route->local_subscribers.has_value()) {
                targets.shm = m_transports.get_send_shm(route->local_subscribers->shm_block);
                targets.shm_signals = route->local_subscribers->subscribe_events;
            }
            
            // snapshot remote subs
            if (!route->remote_subscribers.empty()) {
                targets.sockets.reserve(route->remote_subscribers.size());
                for (const auto remote : route->remote_subscribers) {
                    targets.sockets.push_back(
                        m_transports.get_socket(remote.socket_index)
                    );
                }
            }

            // no one to send to
            if (targets.shm == nullptr &&
                targets.sockets.empty()) return { SendOpErr::None, {}, {} };
        }

        return m_dispatch.dispatch_send(targets, buf, size);
    }

    void Router::recv_from_publisher(Label label, const void* buf, size_t size) {
        if (!buf || size == 0) return;
        
        RecvTargets targets{};
        targets.label = label;
        {
            std::shared_lock lock(m_router_mtx);
            
            const RecvRoute* route = m_routes.get_recv_route(label);
            if (route == nullptr) {
                ERR_PRINT("recv_from_publisher(): unknown label=", label);
                return;
            }

            if (route->label_size != size) {
                ERR_PRINT("recv_from_publisher(): size mismatch label=", label,
                        " expected=", route->label_size, " got=", size);
                return;
            }

            auto uids = m_routes.snapshot_recv_subscribers(label);
            if (uids.empty()) {
                ERR_PRINT("recv_from_publisher(): no recv subscribers for label=", label);
                return;
            }

            targets.subscribers.reserve(uids.size());
            for (handle_uid uid : uids) {
                auto it = m_recv_handles.find(uid);
                if (it != m_recv_handles.end() && it->second) {
                    targets.subscribers.push_back(it->second->data);
                }
            }
        }

        m_dispatch.dispatch_recv_targets(targets, buf, size);
    }
}
