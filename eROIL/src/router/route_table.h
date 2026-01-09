#pragma once
#include <unordered_map>
#include <vector>
#include <optional>
#include <cstddef>

#include "types/types.h"
#include "types/handles.h"

namespace eroil {
 
    struct SendRoute {
        Label label{};
        size_t label_size{};
        std::vector<handle_uid> publishers;
        std::vector<NodeId> remote_subscribers;
        bool has_local_subscribers = false;
    };

    struct RecvRoute {
        Label label{};
        size_t label_size{};
        std::vector<handle_uid> subscribers;
        std::optional<NodeId> remote_publisher;
        bool has_local_publisher = false;
    };

    struct SendPlan {
        Label label{};
        size_t label_size{};
        bool want_local = false;
        std::vector<NodeId> remote_node_ids;
        bool empty() const { return !want_local && remote_node_ids.empty(); }
    };

    class RouteTable {
        private:
            std::unordered_map<Label, SendRoute> m_send_routes;
            std::unordered_map<Label, RecvRoute> m_recv_routes;

        public:
            // send
            bool register_send_publisher(handle_uid uid, Label label, size_t size);
            bool unregister_send_publisher(handle_uid uid, Label label);

            bool add_local_send_subscriber(Label label, size_t size);
            bool add_remote_send_subscriber(Label label, size_t size, NodeId to_id);

            std::optional<SendPlan> build_send_plan(Label label, size_t size) const;

            bool has_send_label(Label label) const;
            std::vector<std::pair<Label, size_t>> get_send_labels() const;

            // has_local_send_subscriber() located in transport registry
            bool is_remote_send_subscriber(Label label, NodeId id) const;
            const SendRoute* find_send_route(Label label) const;

            // recv
            bool register_recv_subscriber(handle_uid uid, Label label, size_t size);
            bool unregister_recv_subscriber(handle_uid uid, Label label);

            bool set_local_recv_publisher(Label label, size_t size);
            bool set_remote_recv_publisher(Label label, size_t size, NodeId from_id);

            std::optional<std::vector<handle_uid>>
            snapshot_recv_subscribers(Label label, size_t size) const;

            bool has_recv_label(Label label) const;
            std::vector<std::pair<Label, size_t>> get_recv_labels() const;

            // has_local_recv_subscriber() located in transport registry
            bool is_remote_recv_publisher(Label label, NodeId from_id) const;
            const RecvRoute* find_recv_route(Label label) const;

        private:
            static void push_unique(std::vector<NodeId>& v, NodeId id);
            static void push_unique(std::vector<handle_uid>& v, handle_uid uid);
            static void erase_value(std::vector<NodeId>& v, NodeId id);
            static void erase_value(std::vector<handle_uid>& v, handle_uid uid);
    };

}
