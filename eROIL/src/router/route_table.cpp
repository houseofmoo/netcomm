#include "route_table.h"
#include <algorithm>

namespace eroil {
    void RouteTable::push_unique(std::vector<NodeId>& v, NodeId id) {
        if (std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
    }

    void RouteTable::push_unique(std::vector<handle_uid>& v, handle_uid uid) {
        if (std::find(v.begin(), v.end(), uid) == v.end()) v.push_back(uid);
    }

    void RouteTable::erase_value(std::vector<NodeId>& v, NodeId id) {
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
    }

    void RouteTable::erase_value(std::vector<handle_uid>& v, handle_uid uid) {
        v.erase(std::remove(v.begin(), v.end(), uid), v.end());
    }

    bool RouteTable::register_send_publisher(handle_uid uid, Label label, size_t size) {
        auto [it, inserted] = m_send_routes.try_emplace(
            label,
            SendRoute{ label, size, {}, {}, false }
        );

        if (!inserted && it->second.label_size != size) {
            return false;
        }

        auto& route = it->second;
        const auto before = route.publishers.size();
        push_unique(route.publishers, uid);

        // if size didnt change, then this was a duplicate
        return route.publishers.size() != before;
    }

    bool RouteTable::unregister_send_publisher(handle_uid uid, Label label) {
        auto it = m_send_routes.find(label);
        if (it == m_send_routes.end()) {
            return false;
        }

        auto& route = it->second;
        const auto before = route.publishers.size();
        erase_value(route.publishers, uid);

        // if we did not find uid, fail
        if (route.publishers.size() == before) {
            return false;
        }

        // if no publishers remain AND no subscribers delete the route
        const bool no_remotes = route.remote_subscribers.empty();
        const bool no_locals  = !route.has_local_subscribers;
        if (route.publishers.empty() && no_remotes && no_locals) {
            m_send_routes.erase(it);
        }

        return true;
    }

    bool RouteTable::add_local_send_subscriber(Label label, size_t size) {
        auto [it, inserted] = m_send_routes.try_emplace(
            label,
            SendRoute{ label, size, {}, {}, false }
        );

        if (!inserted && it->second.label_size != size) {
            return false;
        }

        it->second.has_local_subscribers = true;
        return true;
    }

    bool RouteTable::add_remote_send_subscriber(Label label, size_t size, NodeId to_id) {
        auto [it, inserted] = m_send_routes.try_emplace(
            label,
            SendRoute{ label, size, {}, {}, false }
        );

        if (!inserted && it->second.label_size != size) {
            return false;
        }

        auto& route = it->second;
        const auto before = route.remote_subscribers.size();
        push_unique(route.remote_subscribers, to_id);

        // if size didnt change, then this was a duplicate
        return route.remote_subscribers.size() != before;
    }

    std::optional<SendPlan> RouteTable::build_send_plan(Label label, size_t size) const {
        auto it = m_send_routes.find(label);
        if (it == m_send_routes.end()) {
            return std::nullopt;
        }

        const SendRoute& route = it->second;

        if (route.label_size != size) {
            return std::nullopt;
        }

        if (route.publishers.empty()) {
            // could mean no-op instead, but right now its "failure"
            return std::nullopt;
        }

        return SendPlan{
            label,
            route.label_size,
            route.has_local_subscribers,
            route.remote_subscribers
        };
    }

    bool RouteTable::has_send_label(Label label) const {
        return m_send_routes.find(label) != m_send_routes.end();
    }

    std::vector<std::pair<Label, size_t>> RouteTable::get_send_labels() const {
        std::vector<std::pair<Label, size_t>> out;
        out.reserve(m_send_routes.size());
        for (const auto& [label, route] : m_send_routes) {
            out.emplace_back(label, route.label_size);
        }
        return out;
    }

    bool RouteTable::is_remote_send_subscriber(Label label, NodeId id) const {
        auto it = m_send_routes.find(label);
        if (it == m_send_routes.end()) return false;
        const auto& subscribers = it->second.remote_subscribers;
        return std::find(subscribers.begin(), subscribers.end(), id) != subscribers.end();
    }

    const SendRoute* RouteTable::find_send_route(Label label) const {
        auto it = m_send_routes.find(label);
        return (it == m_send_routes.end()) ? nullptr : &it->second;
    }

    bool RouteTable::register_recv_subscriber(handle_uid uid, Label label, size_t size) {
        auto [it, inserted] = m_recv_routes.try_emplace(
            label,
            RecvRoute{ label, size, {}, std::nullopt, false }
        );

        if (!inserted && it->second.label_size != size) {
            return false;
        }

        auto& route = it->second;
        const auto before = route.subscribers.size();
        push_unique(route.subscribers, uid);

        // if size didnt change, then this was a duplicate
        return route.subscribers.size() != before;
    }

    bool RouteTable::unregister_recv_subscriber(handle_uid uid, Label label) {
        auto it = m_recv_routes.find(label);
        if (it == m_recv_routes.end()) {
            return false;
        }

        auto& route = it->second;
        const auto before = route.subscribers.size();
        erase_value(route.subscribers, uid);

        if (route.subscribers.size() == before) {
            return false; 
        }

        // if no subscribers remain AND no publisher (local/remote), delete the route
        const bool no_remote_pub = !route.remote_publisher.has_value();
        const bool no_local_pub  = !route.has_local_publisher;
        if (route.subscribers.empty() && no_remote_pub && no_local_pub) {
            m_recv_routes.erase(it);
        }

        return true;
    }

    bool RouteTable::set_local_recv_publisher(Label label, size_t size) {
        auto [it, inserted] = m_recv_routes.try_emplace(
            label,
            RecvRoute{ label, size, {}, std::nullopt, false }
        );

        if (!inserted && it->second.label_size != size) {
            return false;
        }

        // local publisher overrides remote publisher
        it->second.remote_publisher = std::nullopt;
        it->second.has_local_publisher = true;
        return true;
    }

    bool RouteTable::set_remote_recv_publisher(Label label, size_t size, NodeId from_id) {
        auto [it, inserted] = m_recv_routes.try_emplace(
            label,
            RecvRoute{ label, size, {}, std::nullopt, false }
        );

        if (!inserted && it->second.label_size != size) {
            return false;
        }

        auto& route = it->second;

        // If local publisher exists, remote publisher should not be set.
        if (route.has_local_publisher) {
            return false;
        }

        // If remote already set:
        if (route.remote_publisher.has_value()) {
            // same publisher is a no-op success (you did that in your code)
            if (*route.remote_publisher == from_id) return true;
            // different publisher is rejected
            return false;
        }

        route.remote_publisher = from_id;
        return true;
    }

    std::optional<std::vector<handle_uid>>
    RouteTable::snapshot_recv_subscribers(Label label, size_t size) const {
        auto it = m_recv_routes.find(label);
        if (it == m_recv_routes.end()) return std::nullopt;
        const RecvRoute& r = it->second;
        if (r.label_size != size) return std::nullopt;
        return r.subscribers;
    }

    bool RouteTable::has_recv_label(Label label) const {
        return m_recv_routes.find(label) != m_recv_routes.end();
    }

    std::vector<std::pair<Label, size_t>> RouteTable::get_recv_labels() const {
        std::vector<std::pair<Label, size_t>> out;
        out.reserve(m_recv_routes.size());
        for (const auto& [label, route] : m_recv_routes) {
            out.emplace_back(label, route.label_size);
        }
        return out;
    }

    bool RouteTable::is_remote_recv_publisher(Label label, NodeId from_id) const {
        auto it = m_recv_routes.find(label);
        if (it == m_recv_routes.end()) return false;
        const auto& publisher = it->second.remote_publisher;
        return publisher.has_value() && (*publisher == from_id);
    }

    const RecvRoute* RouteTable::find_recv_route(Label label) const {
        auto it = m_recv_routes.find(label);
        return (it == m_recv_routes.end()) ? nullptr : &it->second;
    }
}