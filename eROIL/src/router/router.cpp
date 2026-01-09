#include "router.h"

#include <eROIL/print.h>

namespace eroil {
    static bool should_close_send_shm(const SendRoute& r) {
        const bool no_publishers = r.publishers.empty();
        const bool no_subscribers = (!r.has_local_subscribers) && r.remote_subscribers.empty();
        return no_publishers && no_subscribers;
    }

    static bool should_close_recv_shm(const RecvRoute& r) {
        const bool no_subscribers = r.subscribers.empty();
        const bool no_publishers = (!r.has_local_publisher) && (!r.remote_publisher.has_value());
        return no_subscribers && no_publishers;
    }

    Router::Router() = default;
    Router::~Router() = default;

    void Router::register_send_publisher(std::unique_ptr<SendHandle> handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        const handle_uid uid = handle->uid;
        const Label label = handle->data.label;
        const size_t size = handle->data.buf_size;

        // store handle
        auto [handles_it, inserted] = m_send_handles.try_emplace(uid, std::move(handle));
        if (!inserted) {
            ERR_PRINT("register_send_publisher(): duplicate send handle uid=", uid);
            return;
        }

        // update routes
        if (!m_routes.register_send_publisher(uid, label, size)) {
            ERR_PRINT("register_send_publisher(): size mismatch or duplicate publisher for label=", label);
            m_send_handles.erase(uid);
            return;
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

        const Label label = handle->data.label;
        const handle_uid uid = handle->uid;

        // erase handle first
        m_send_handles.erase(it);

        // remove from route table (removes uid from publishers; may delete route if unused)
        const bool ok = m_routes.unregister_send_publisher(uid, label);
        if (!ok) {
            ERR_PRINT("unregister_send_publisher(): route table did not contain publisher uid=", uid, " label=", label);
            // continue anyway
        }

        // if route was deleted, close the shared memory block
        const SendRoute* route = m_routes.find_send_route(label);
        if (route == nullptr || should_close_send_shm(*route)) {
            m_transports.erase_send_shm(label);
        }
    }

    void Router::register_recv_subscriber(std::unique_ptr<RecvHandle> handle) {
        if (!handle) return;

        std::unique_lock lock(m_router_mtx);

        const handle_uid uid = handle->uid;
        const Label label = handle->target->data.label;
        const size_t size = handle->target->data.buf_size;

        // store recv target
        auto target = handle->target;
        auto [t_it, t_inserted] = m_recv_targets.try_emplace(uid, target);
        if (!t_inserted) {
            ERR_PRINT("register_recv_subscriber(): duplicate recv target uid=", uid);
            return;
        }

        // store handle
        auto [h_it, h_inserted] = m_recv_handles.try_emplace(uid, std::move(handle));
        if (!h_inserted) {
            ERR_PRINT("register_recv_subscriber(): duplicate recv handle uid=", uid);
            return;
        }

        // update routes
        if (!m_routes.register_recv_subscriber(uid, label, size)) {
            ERR_PRINT("register_recv_subscriber(): size mismatch or duplicate subscriber for label=", label);
            m_recv_handles.erase(uid);
            return;
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
        const Label label = handle->target->data.label;

        // remove from route table (removes uid from subscribers; may delete route if unused)
        const bool ok = m_routes.unregister_recv_subscriber(uid, label);
        if (!ok) {
            ERR_PRINT("unregister_recv_subscriber(): route table did not contain subscriber uid=", uid, " label=", label);
        }

        // erase handle
        m_recv_targets.erase(uid);
        m_recv_handles.erase(uid);

        // if route was deleted, close the shared memory block
        const RecvRoute* route = m_routes.find_recv_route(label);
        
        if (route == nullptr || should_close_recv_shm(*route)) {
            m_transports.erase_recv_shm(label);
        }
    }

    void Router::add_local_send_subscriber(Label label, size_t size, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_routes.add_local_send_subscriber(label, size)) {
            ERR_PRINT("add_local_send_subscriber(): size mismatch label=", label);
            return;
        }

        // ensure send shm block exists/opened and event is set
        auto shm = m_transports.ensure_send_shm(label, size, to_id);
        if (!shm) {
            ERR_PRINT("add_local_send_subscriber(): failed to ensure send shm label=", label, " to_id=", to_id);
            return;
        }
    }

    void Router::add_remote_send_subscriber(Label label, size_t size, NodeId to_id) {
        std::unique_lock lock(m_router_mtx);

        // Socket must exist (matches your old behavior)
        if (!m_transports.has_socket(to_id)) {
            ERR_PRINT("add_remote_send_subscriber(): missing socket to NodeId=", to_id);
            return;
        }

        if (!m_routes.add_remote_send_subscriber(label, size, to_id)) {
            ERR_PRINT("add_remote_send_subscriber(): size mismatch or duplicate remote sub label=", label, " to_id=", to_id);
            return;
        }
    }

    void Router::set_local_recv_publisher(Label label, size_t size, NodeId my_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_routes.set_local_recv_publisher(label, size)) {
            ERR_PRINT("set_local_recv_publisher(): size mismatch label=", label);
            return;
        }

        // ensure recv shm exists/opened and event is set
        auto shm = m_transports.ensure_recv_shm(label, size, my_id);
        if (!shm) {
            ERR_PRINT("set_local_recv_publisher(): failed to ensure recv shm label=", label, " my_id=", my_id);
            return;
        }
    }

    void Router::set_remote_recv_publisher(Label label, size_t size, NodeId from_id) {
        std::unique_lock lock(m_router_mtx);

        if (!m_transports.has_socket(from_id)) {
            ERR_PRINT("set_remote_recv_publisher(): missing socket from NodeId=", from_id);
            return;
        }

        if (!m_routes.set_remote_recv_publisher(label, size, from_id)) {
            ERR_PRINT("set_remote_recv_publisher(): rejected (local publisher exists or overwrite) label=", label, " from_id=", from_id);
            return;
        }
    }

    SendOpErr Router::send_to_subscribers(Label label, const void* buf, size_t size) {
        if (!buf || size == 0) return SendOpErr::Failed;

        SendPlan plan{};
        SendTargets targets{};

        {
            std::shared_lock lock(m_router_mtx);

            const SendRoute* route = m_routes.find_send_route(label);
            if (route == nullptr) {
                ERR_PRINT("send_to_subscribers(): unknown label=", label);
                return SendOpErr::RouteNotFound;
            }

            if (route->label_size != size) {
                ERR_PRINT("send_to_subscribers(): size mismatch label=", label,
                          " expected=", route->label_size, " got=", size);
                return SendOpErr::SizeMismatch;
            }

            if (route->publishers.empty()) {
                ERR_PRINT("send_to_subscribers(): called with no publishers label=", label);
                return SendOpErr::Failed;
            }

            auto plan_opt = m_routes.build_send_plan(label, size);
            if (!plan_opt.has_value()) {
                ERR_PRINT("send_to_subscribers(): failed to build send plan label=", label);
                return SendOpErr::Failed;
            }

            plan = std::move(*plan_opt);
            targets = m_transports.resolve_send_targets(label, plan.want_local, plan.remote_node_ids);

            // no one to send to, we're done
            if (!plan.want_local && plan.remote_node_ids.empty()) {
                return SendOpErr::None;
            }
        }

        return m_dispatch.dispatch_send(plan, targets, buf, size);
    }

    void Router::recv_from_publisher(Label label, const void* buf, size_t size) {
        if (!buf || size == 0) return;

        size_t label_size = 0;
        std::vector<std::shared_ptr<RecvTarget>> targets;
        {
            std::shared_lock lock(m_router_mtx);
            
            const RecvRoute* route = m_routes.find_recv_route(label);
            if (route == nullptr) {
                ERR_PRINT("recv_from_publisher(): unknown label=", label);
                return;
            }

            label_size = route->label_size;
            if (label_size != size) {
                ERR_PRINT("recv_from_publisher(): size mismatch label=", label,
                        " expected=", route->label_size, " got=", size);
                return;
            }

            auto uids_opt = m_routes.snapshot_recv_subscribers(label, size);
            if (!uids_opt.has_value()) {
                ERR_PRINT("recv_from_publisher(): failed to snapshot sub uids for label=", label);
                return;
            }

            targets = snapshot_recv_delivery(*uids_opt);
        }

        m_dispatch.dispatch_recv_targets(label, label_size, buf, size, targets);
    }

    SendHandle* Router::get_send_handle(handle_uid uid) const {
        std::shared_lock lock(m_router_mtx);
        auto it = m_send_handles.find(uid);
        if (it == m_send_handles.end()) return nullptr;
        return it->second.get();
    }

    RecvHandle* Router::get_recv_handle(handle_uid uid) const {
        std::shared_lock lock(m_router_mtx);
        auto it = m_recv_handles.find(uid);
        if (it == m_recv_handles.end()) return nullptr;
        return it->second.get();
    }

    std::vector<std::pair<Label, size_t>> Router::get_send_labels() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_send_labels();
    }

    std::vector<std::pair<Label, size_t>> Router::get_recv_labels() const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.get_recv_labels();
    }

    bool Router::has_send_label(Label label) const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.has_send_label(label);
    }

    bool Router::has_recv_label(Label label) const {
        std::shared_lock lock(m_router_mtx);
        return m_routes.has_recv_label(label);
    }

    bool Router::is_send_subscriber(Label label, NodeId id) const {
        std::shared_lock lock(m_router_mtx);

        if (m_routes.is_remote_send_subscriber(label, id)) {
            return true;
        }

        return m_transports.has_local_send_subscriber(label, id);
    }

    bool Router::is_recv_publisher(Label label, NodeId from_id, NodeId my_id) const {
        std::shared_lock lock(m_router_mtx);

        if (m_routes.is_remote_recv_publisher(label, from_id)) {
            return true;
        }

        return m_transports.has_local_recv_publisher(label, my_id);
    }

    std::vector<std::shared_ptr<RecvTarget>>
    Router::snapshot_recv_delivery(const std::vector<handle_uid>& uids) const {
        std::vector<std::shared_ptr<RecvTarget>> out;
        out.reserve(uids.size());

        for (handle_uid uid : uids) {
            auto it = m_recv_targets.find(uid);
            if (it != m_recv_targets.end() && it->second) {
                out.push_back(it->second);
            }
        }
        return out;
    }
}
