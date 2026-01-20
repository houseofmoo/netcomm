#include "route_table.h"
#include <algorithm>
#include <eROIL/print.h>

namespace eroil {
    static bool is_empty(const std::variant<std::monostate, RemotePublisher, LocalPublisher>& publisher) {
        return std::holds_alternative<std::monostate>(publisher);
    }

    // static bool is_remote(const std::variant<std::monostate, RemotePublisher, LocalPublisher>& publisher) {
    //     return std::holds_alternative<RemotePublisher>(publisher);
    // }

    static bool is_local(const std::variant<std::monostate, RemotePublisher, LocalPublisher>& publisher) {
        return std::holds_alternative<LocalPublisher>(publisher);
    }

    SendRoute* RouteTable::require_send_route(Label label, const char* fn) {
        auto* route = get_send_route(label);
        if (!route) {
            ERR_PRINT(fn, "(): send route not found, label=", label);
        }
        return route;
    }

    RecvRoute* RouteTable::require_recv_route(Label label, const char* fn) {
        auto* route = get_recv_route(label);
        if (!route) {
            ERR_PRINT(fn, "(): recv route not found, label=", label);
        }
        return route;
    }

    bool RouteTable::require_route_size(size_t expected, size_t actual, const char* fn) {
        if (expected != actual) {
            ERR_PRINT(fn, "(): size mismatch, expected=", expected, ", actual=", actual);
            return false;
        }
        return true;
    }

    std::array<LabelInfo, MAX_LABELS> RouteTable::get_send_labels() const {
        std::array<LabelInfo, MAX_LABELS> labels;
        labels.fill(LabelInfo{ INVALID_LABEL, 0 });

        size_t index = 0;
        for (const auto& [label, route] : m_send_routes) {
            if (index >= MAX_LABELS) {
                ERR_PRINT(__func__, "(): too many send labels for broadcast message to send");
                break;
            }
            labels[index++] = LabelInfo{ label, static_cast<uint32_t>(route.label_size) };
        }
        return labels;
    }

    std::array<LabelInfo, MAX_LABELS> RouteTable::get_recv_labels() const {
        std::array<LabelInfo, MAX_LABELS> labels;
        labels.fill(LabelInfo{ INVALID_LABEL, 0 });
        
        size_t index = 0;
        for (const auto& [label, route] : m_recv_routes) {
            if (index >= MAX_LABELS) {
                ERR_PRINT(__func__, "(): too many recv labels for broadcast message to send");
                break;
            }
            labels[index++] = LabelInfo{ label, static_cast<uint32_t>(route.label_size) };
        }
        return labels;
    }

    // send route
    void RouteTable::create_send_route(Label label, SendHandle* handle) {
        auto [it, inserted] = m_send_routes.emplace(
            label,
            SendRoute{
                label,
                handle->data->buf_size,
                {},
                {},
                std::nullopt,
            }
        );
        (void)it;

        if (!inserted) {
            ERR_PRINT(__func__, "(): failed to insert new send route");
        }
    }

    bool RouteTable::add_send_publisher(Label label, SendHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT(__func__, "(): given a null handle");
            return false;
        }
        
        if (!has_send_route(label)) {
            create_send_route(label, handle);
        }

        auto route = require_send_route(label, __func__);
        if (route == nullptr) return false;
        if (!require_route_size(route->label_size, handle->data->buf_size, __func__)) return false;

        auto it = std::find(
            route->publishers.begin(),
            route->publishers.end(),
            handle->uid
        );

        if (it != route->publishers.end()) {
            ERR_PRINT(__func__, "(): publisher already exists in publishers list, uid=", handle->uid);
            return false;
        }

        route->publishers.push_back(handle->uid);
        return true;
    }

    bool RouteTable::remove_send_publisher(Label label,  handle_uid uid) {
        auto route = require_send_route(label, __func__);
        if (route == nullptr) return false;

        auto it = std::find(
            route->publishers.begin(),
            route->publishers.end(),
            uid
        );

        if (it == route->publishers.end()) {
            ERR_PRINT(__func__, "(): not a send publisher, label=", label, ", uid=", uid);
            return false;
        }
        route->publishers.erase(it);

        if (route->publishers.empty()) {
            m_send_routes.erase(label);
        }

        return true;
    }

    bool RouteTable::add_local_send_subscriber(Label label, size_t size, NodeId my_id, NodeId to_id) {
        auto route = require_send_route(label, __func__);
        if (route == nullptr) return false;

        if (is_local_send_subscriber(label, to_id)) {
            ERR_PRINT(__func__, "(): already a send subscriber, label=", label, ", to_id=", to_id);
            return false;
        }

        if (!require_route_size(route->label_size, size, __func__)) return false;

        if (!route->local_subscribers.has_value()) {
            route->local_subscribers = { label, {} };
        }

        auto it = route->local_subscribers->subscribe_events.emplace_back(
            std::make_shared<evt::NamedEvent>(label, my_id, to_id)
        );

        if (route->local_subscribers->shm_id != label) {
            ERR_PRINT(__func__, "(): had a different shm block index, label=", label,
                      ", shm_index=", route->local_subscribers->shm_id);
        }

        return true;
    }

    bool RouteTable::remove_local_send_subscriber(Label label, NodeId to_id) {
        auto route = require_send_route(label, __func__);
        if (route == nullptr) return false;

        if (!route->local_subscribers.has_value()) {
            ERR_PRINT(__func__, "(): local send subscriber is unexpectedly empty");
            return false;
        }

        auto it = std::find_if(
            route->local_subscribers->subscribe_events.begin(),
            route->local_subscribers->subscribe_events.end(),
            [&](const std::shared_ptr<evt::NamedEvent> e) {
                auto info = e->get_info();
                return info.dst_id == to_id;
            }
        );

        if (it == route->local_subscribers->subscribe_events.end()) {
            ERR_PRINT(__func__, "(): event not found, label=", label, ", to_id=", to_id);
            return false;
        }

        //(*it)->close(); // ensure event is closed and RAII does not keep it alive
        route->local_subscribers->subscribe_events.erase(it);
        return true;
    }

    bool RouteTable::add_remote_send_subscriber(Label label, size_t size, NodeId to_id) {
        auto route = require_send_route(label, __func__);
        if (route == nullptr) return false;
   
        if (!require_route_size(route->label_size, size, __func__)) return false;


        auto it = std::find_if(
            route->remote_subscribers.begin(),
            route->remote_subscribers.end(),
            [&](const RemoteSubscriber& r) {
                return r.socket_id == to_id;
            }
        );

        if (it == route->remote_subscribers.end()) {
            route->remote_subscribers.emplace_back(
                RemoteSubscriber{ to_id }
            );
            return true;
        }

        ERR_PRINT(__func__, "(): subscriber already exists in send subscriber list, label=", label, ", to_id=", to_id);
        return false;
    }

    bool RouteTable::remove_remote_send_subscriber(Label label, NodeId to_id) {
        auto route = require_send_route(label, __func__);
        if (route == nullptr) return false;

        auto it = std::find_if(
            route->remote_subscribers.begin(),
            route->remote_subscribers.end(),
            [&](const RemoteSubscriber& r) {
                return r.socket_id == to_id;
            }
        );

        if (it != route->remote_subscribers.end()) {
            route->remote_subscribers.erase(it);
            return true;
        }

        ERR_PRINT(__func__, "(): not a remote send subscriber, label=", label, ", to_id=", to_id);
        return false;
    }

    const SendRoute* RouteTable::get_send_route(Label label) const noexcept {
        auto it = m_send_routes.find(label);
        if (it == m_send_routes.end()) return nullptr;
        return &it->second;
    }

    SendRoute* RouteTable::get_send_route(Label label) noexcept {
        auto it = m_send_routes.find(label);
        if (it == m_send_routes.end()) return nullptr;
        return &it->second;
    }
    
    bool RouteTable::has_send_route(Label label) const noexcept {
        auto it = m_send_routes.find(label);
        return it != m_send_routes.end();
    }

    bool RouteTable::is_send_publisher(Label label, handle_uid uid) const noexcept {
        auto route = get_send_route(label);
        if (route == nullptr) return false;

        auto it = std::find(
            route->publishers.begin(),
            route->publishers.end(),
            uid
        );
        return it != route->publishers.end();
    }

    bool RouteTable::is_local_send_subscriber(Label label, NodeId to_id) const noexcept {
        auto route = get_send_route(label);
        if (route == nullptr) return false;
        if (!route->local_subscribers.has_value()) return false;

        auto it = std::find_if(
            route->local_subscribers->subscribe_events.begin(),
            route->local_subscribers->subscribe_events.end(),
            [&](const std::shared_ptr<evt::NamedEvent> e) {
                auto info = e->get_info();
                return info.dst_id == to_id;
            }
        );
        return it != route->local_subscribers->subscribe_events.end();
    }

    bool RouteTable::is_remote_send_subscriber(Label label, NodeId to_id) const noexcept {
        auto route = get_send_route(label);
        if (route == nullptr) return false;

        auto it = std::find_if(
            route->remote_subscribers.begin(),
            route->remote_subscribers.end(),
            [&](const RemoteSubscriber& r) {
                return r.socket_id == to_id;
            }
        );
        return it != route->remote_subscribers.end();
    }

    // recv route
    void RouteTable::create_recv_route(Label label, RecvHandle* handle) {
        auto [it, inserted] = m_recv_routes.emplace(
            label,
            RecvRoute{
                label,
                handle->data->buf_size,
                {},
                std::monostate{}
            }
        );
        (void)it;

        if (!inserted) {
            ERR_PRINT(__func__, "(): failed to insert new recv route");
        }
    }

    bool RouteTable::add_recv_subscriber(Label label, RecvHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT(__func__, "(): given a null handle");
            return false;
        }

        if (!has_recv_route(label)) {
            create_recv_route(label, handle);
        }

        auto route = require_recv_route(label, __func__);
        if (route == nullptr) return false;
        if (!require_route_size(route->label_size, handle->data->buf_size, __func__)) return false;

        auto it = std::find(
            route->subscribers.begin(),
            route->subscribers.end(),
            handle->uid
        );

        if (it != route->subscribers.end()) {
            ERR_PRINT(__func__, "(): handle already in subscribers list, uid=", handle->uid);
            return false;
        }

        route->subscribers.push_back(handle->uid);
        return true;
    }
    
    bool RouteTable::remove_recv_subscriber(Label label, handle_uid uid) {
        auto route = require_recv_route(label, __func__);
        if (route == nullptr) return false;

        auto it = std::find(
            route->subscribers.begin(),
            route->subscribers.end(),
            uid
        );

        if (it == route->subscribers.end()) {
            ERR_PRINT(__func__, "(): uid not in subscribers list, label=", label, ", uid=", uid);
            return false;
        }

        route->subscribers.erase(it);

        if (route->subscribers.empty()) {
            m_recv_routes.erase(label);
        }

        return true;
    }

    bool RouteTable::add_local_recv_publisher(Label label, size_t size, NodeId my_id, NodeId from_id) {
        auto route = require_recv_route(label, __func__);
        if (route == nullptr) return false;
        
        if (is_local_recv_publisher(label, from_id)) {
            ERR_PRINT(__func__, "(): already a local recv publisher, label=", label, ", from_id=", from_id);
            return false;
        }

        if (!require_route_size(route->label_size, size, __func__)) return false;

        route->publisher = LocalPublisher{
            label,
            std::make_shared<evt::NamedEvent>(label, from_id, my_id)
        };
        return true;
    }

    bool RouteTable::remove_local_recv_publisher(Label label, NodeId from_id) {
        auto route = require_recv_route(label, __func__);
        if (route == nullptr) return false;

        auto* pub = std::get_if<LocalPublisher>(&route->publisher);
        if (pub == nullptr || pub->publish_event == nullptr) {
            ERR_PRINT(__func__, "(): not a local publisher, label=", label);
            return false;
        }

        if (pub->publish_event->get_info().src_id != from_id) {
            ERR_PRINT(__func__, "(): from id does not match expected, label=", label);
            return false;
        }

        pub->publish_event->close();
        route->publisher = std::monostate{};
        return true;
    }

    bool RouteTable::add_remote_recv_publisher(Label label, size_t size, NodeId from_id) {
        auto route = require_recv_route(label, __func__);
        if (route == nullptr) return false;

        if (!require_route_size(route->label_size, size, __func__)) return false;

        if (is_empty(route->publisher)) {
            route->publisher = RemotePublisher{ from_id };
            return true;
        }

        if (is_local(route->publisher)) {
            ERR_PRINT(__func__, "(): tried to overwrite a local recv publisher with remote");
            return false;
        }

        // must be remote, log the fact we tried to change it cause that shouldnt happen
        ERR_PRINT(__func__, "(): tried to overwrite remote recv publisher with a new remote recv pubisher");
        return false;
    }
    
    bool RouteTable::remove_remote_recv_publisher(Label label, NodeId from_id) {
        auto route = require_recv_route(label, __func__);
        if (route == nullptr) return false;

        auto* ptr = std::get_if<RemotePublisher>(&route->publisher);
        if (ptr == nullptr) {
            ERR_PRINT(__func__, "(): not remote recv publisher, label=", label, ", from_id=", from_id);
            return false;
        }

        if (ptr->socket_id != from_id) {
            ERR_PRINT(__func__, "(): id's do not match");
            return false;
        }

        route->publisher = std::monostate{};
        return true;
    }

    const RecvRoute* RouteTable::get_recv_route(Label label) const noexcept {
        auto it = m_recv_routes.find(label);
        if (it == m_recv_routes.end()) return nullptr;
        return &it->second;
    }

    RecvRoute* RouteTable::get_recv_route(Label label) noexcept {
        auto it = m_recv_routes.find(label);
        if (it == m_recv_routes.end()) return nullptr;
        return &it->second;
    }
    
    bool RouteTable::has_recv_route(Label label) const noexcept {
        auto it = m_recv_routes.find(label);
        return it != m_recv_routes.end();
    }

    bool RouteTable::is_recv_subscriber(Label label, handle_uid uid) const noexcept {
        auto route = get_recv_route(label);
        if (route == nullptr) return false;

        auto it = std::find(
            route->subscribers.begin(),
            route->subscribers.end(),
            uid
        );
        return it != route->subscribers.end();
    }

    bool RouteTable::is_local_recv_publisher(Label label, NodeId from_id) const noexcept {
        auto route = get_recv_route(label);
        if (route == nullptr) return false;

        if (auto* local = std::get_if<LocalPublisher>(&route->publisher)) {
            return local->publish_event->get_info().src_id == from_id;
        }
        return false;
    }

    bool RouteTable::is_remote_recv_publisher(Label label, NodeId from_id) const noexcept {
        auto route = get_recv_route(label);
        if (route == nullptr) return false;

        if (auto* remote = std::get_if<RemotePublisher>(&route->publisher)) {
            return remote->socket_id == from_id;
        }
        return false;
    }

    std::vector<handle_uid>
    RouteTable::snapshot_send_publishers(Label label) const {
        auto it = m_send_routes.find(label);
        if (it == m_send_routes.end()) {
            ERR_PRINT("snapshot_send_publishers(): send route not found, label=", label);
            return {};
        }
        return it->second.publishers;
    }

    std::vector<handle_uid>
    RouteTable::snapshot_recv_subscribers(Label label) const {
        auto it = m_recv_routes.find(label);
        if (it == m_recv_routes.end()) {
            ERR_PRINT("snapshot_recv_subscribers(): recv route not found, label=", label);
            return {};
        }
        return it->second.subscribers;
    }
}
