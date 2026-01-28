#include "route_table.h"
#include <algorithm>
#include <eROIL/print.h>
#include "types/label_io_types.h"

namespace eroil {
    io::LabelsSnapshot RouteTable::get_send_labels_snapshot() const {
        io::LabelsSnapshot snapshot{};
        snapshot.gen = m_send_gen.load(std::memory_order_relaxed);
        snapshot.labels = get_send_labels_sorted();
        return snapshot;
    }

    io::LabelsSnapshot RouteTable::get_recv_labels_snapshot() const {
        io::LabelsSnapshot snapshot{};
        snapshot.gen = m_recv_gen.load(std::memory_order_relaxed);
        snapshot.labels = get_recv_labels_sorted();
        return snapshot;
    }

    std::array<io::LabelInfo, MAX_LABELS> RouteTable::get_send_labels() const {
        std::array<io::LabelInfo, MAX_LABELS> labels{};

        size_t index = 0;
        for (const auto& [label, route] : m_send_routes) {
            if (index >= MAX_LABELS) {
                ERR_PRINT("(): too many send labels for broadcast message to send");
                break;
            }
            labels[index].label = label;
            labels[index].size = static_cast<uint32_t>(route.label_size);
            index += 1;
        }

        return labels;
    }

    std::array<io::LabelInfo, MAX_LABELS> RouteTable::get_recv_labels() const {
        std::array<io::LabelInfo, MAX_LABELS> labels{};

        size_t index = 0;
        for (const auto& [label, route] : m_recv_routes) {
            if (index >= MAX_LABELS) {
                ERR_PRINT("(): too many recv labels for broadcast message to send");
                break;
            }
            labels[index].label = label;
            labels[index].size = static_cast<uint32_t>(route.label_size);
            index += 1;
        }

        return labels;
    }

    std::array<io::LabelInfo, MAX_LABELS> RouteTable::get_send_labels_sorted() const {
        std::array<io::LabelInfo, MAX_LABELS> labels = get_send_labels();
        std::sort(
            labels.begin(), 
            labels.end(),
            [](const io::LabelInfo& a, const io::LabelInfo& b) {
                return a.label < b.label;
            }
        );

        return labels;
    }

    std::array<io::LabelInfo, MAX_LABELS> RouteTable::get_recv_labels_sorted() const {
        std::array<io::LabelInfo, MAX_LABELS> labels = get_recv_labels();
        std::sort(
            labels.begin(), 
            labels.end(),
            [](const io::LabelInfo& a, const io::LabelInfo& b) {
                return a.label < b.label;
            }
        );

        return labels;
    }

    // send route
    void RouteTable::create_send_route(Label label, hndl::SendHandle* handle) {
        auto [it, inserted] = m_send_routes.emplace(
            label,
            SendRoute{ label, handle->data->buf_size, {}, {}, {} }
        );
        (void)it;

        if (!inserted) {
            ERR_PRINT("(): failed to insert new send route");
            return;
        }
    }

    bool RouteTable::add_send_publisher(Label label, hndl::SendHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT("(): given a null handle");
            return false;
        }
        
        if (!has_send_route(label)) {
            create_send_route(label, handle);
            ++m_send_gen;
        }

        auto* route = get_send_route(label);
        if (route == nullptr) {
            ERR_PRINT("send route not found, label=", label);
            return false;
        }

        if (route->label_size != handle->data->buf_size) {
            ERR_PRINT("size mismatch, expected=", route->label_size, ", actual=", handle->data->buf_size);
            return false;
        }

        auto it = std::find(
            route->publishers.begin(),
            route->publishers.end(),
            handle->uid
        );

        if (it != route->publishers.end()) {
            ERR_PRINT("(): publisher already exists in publishers list, uid=", handle->uid);
            return false;
        }

        route->publishers.push_back(handle->uid);
        return true;
    }

    bool RouteTable::remove_send_publisher(Label label,  handle_uid uid) {
        auto* route = get_send_route(label);
        if (route == nullptr) {
            ERR_PRINT("send route not found, label=", label);
            return false;
        }

        auto it = std::find(
            route->publishers.begin(),
            route->publishers.end(),
            uid
        );

        if (it == route->publishers.end()) {
            ERR_PRINT("(): not a send publisher, label=", label, ", uid=", uid);
            return false;
        }
        route->publishers.erase(it);

        // if no one publishes this route, erase it
        if (route->publishers.empty()) {
            m_send_routes.erase(label);
            ++m_send_gen;
        }

        return true;
    }

    bool RouteTable::add_local_send_subscriber(Label label, size_t size, NodeId dst_id) {
        auto* route = get_send_route(label);
        if (route == nullptr) {
            ERR_PRINT("send route not found, label=", label);
            return false;
        }

        if (route->label_size != size) {
            ERR_PRINT("size mismatch, expected=", route->label_size, ", actual=", size);
            return false;
        }

        if (is_local_send_subscriber(label, dst_id)) {
            ERR_PRINT("(): already a send subscriber, label=", label, ", to_id=", dst_id);
            return false;
        }

        route->local_subscribers.push_back(dst_id);
        return true;
    }

    bool RouteTable::remove_local_send_subscriber(Label label, NodeId dst_id) {
        auto* route = get_send_route(label);
        if (route == nullptr) {
            ERR_PRINT("send route not found, label=", label);
            return false;
        }

        auto it = std::find(
            route->local_subscribers.begin(),
            route->local_subscribers.end(),
            dst_id
        );

        if (it == route->local_subscribers.end()) {
            ERR_PRINT("(): event not found, label=", label, ", to_id=", dst_id);
            return false;
        }

        route->local_subscribers.erase(it);
        return true;
    }

    bool RouteTable::add_remote_send_subscriber(Label label, size_t size, NodeId dst_id) {
        auto* route = get_send_route(label);
        if (route == nullptr) {
            ERR_PRINT("send route not found, label=", label);
            return false;
        }
   
        if (route->label_size != size) {
            ERR_PRINT("size mismatch, expected=", route->label_size, ", actual=", size);
            return false;
        }

        if (is_remote_send_subscriber(label, dst_id)) {
            ERR_PRINT("(): already a send subscriber, label=", label, ", to_id=", dst_id);
            return false;
        }

        route->remote_subscribers.push_back(dst_id);
        return true;
    }

    bool RouteTable::remove_remote_send_subscriber(Label label, NodeId dst_id) {
        auto* route = get_send_route(label);
        if (route == nullptr) {
            ERR_PRINT("send route not found, label=", label);
            return false;
        }

        auto it = std::find(
            route->remote_subscribers.begin(),
            route->remote_subscribers.end(),
            dst_id
        );

        if (it == route->remote_subscribers.end()) {
            ERR_PRINT("(): not a remote send subscriber, label=", label, ", to_id=", dst_id);
            return false;
        }
        route->remote_subscribers.erase(it);
        return true;
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

    bool RouteTable::is_local_send_subscriber(Label label, NodeId dst_id) const noexcept {
        auto route = get_send_route(label);
        if (route == nullptr) return false;
        if (route->local_subscribers.empty()) return false;

        auto it = std::find(
            route->local_subscribers.begin(),
            route->local_subscribers.end(),
            dst_id
        );
        return it != route->local_subscribers.end();
    }

    bool RouteTable::is_remote_send_subscriber(Label label, NodeId dst_id) const noexcept {
        auto route = get_send_route(label);
        if (route == nullptr) return false;
        if (route->remote_subscribers.empty()) return false;

        auto it = std::find(
            route->remote_subscribers.begin(),
            route->remote_subscribers.end(),
            dst_id
        );
        return it != route->remote_subscribers.end();
    }

    // recv route
    void RouteTable::create_recv_route(Label label, hndl::RecvHandle* handle) {
        auto [it, inserted] = m_recv_routes.emplace(
            label,
            RecvRoute{ label, handle->data->buf_size, {} }
        );
        (void)it;

        if (!inserted) {
            ERR_PRINT("(): failed to insert new recv route");
            return;
        }
    }

    bool RouteTable::add_recv_subscriber(Label label, hndl::RecvHandle* handle) {
        if (handle == nullptr) {
            ERR_PRINT("(): given a null handle");
            return false;
        }

        if (!has_recv_route(label)) {
            create_recv_route(label, handle);
            ++m_recv_gen;
        }

        auto* route = get_recv_route(label);
        if (route == nullptr) {
            ERR_PRINT("recv route not found, label=", label);
            return false;
        }

        if (route->label_size != handle->data->buf_size) {
            ERR_PRINT("size mismatch, expected=", route->label_size, ", actual=", handle->data->buf_size);
            return false;
        }

        auto it = std::find(
            route->subscribers.begin(),
            route->subscribers.end(),
            handle->uid
        );

        if (it != route->subscribers.end()) {
            ERR_PRINT("(): subscriber already exists in subscribers list, uid=", handle->uid);
            return false;
        }

        route->subscribers.push_back(handle->uid);
        return true;
    }
    
    bool RouteTable::remove_recv_subscriber(Label label, handle_uid uid) {
        auto* route = get_recv_route(label);
        if (route == nullptr) {
            ERR_PRINT("recv route not found, label=", label);
            return false;
        }

        auto it = std::find(
            route->subscribers.begin(),
            route->subscribers.end(),
            uid
        );

        if (it == route->subscribers.end()) {
            ERR_PRINT("(): not a recv subscriber, label=", label, ", uid=", uid);
            return false;
        }

        route->subscribers.erase(it);

        if (route->subscribers.empty()) {
            m_recv_routes.erase(label);
            ++m_recv_gen;
        }

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
