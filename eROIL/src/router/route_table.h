#pragma once
#include <unordered_map>
#include <vector>
#include <optional>
#include <cstddef>
#include <variant>

#include "events/named_event.h"
#include "types/types.h"
#include "types/handles.h"

namespace eroil {
    struct LocalSubscriber {
        Label shm_block{};
        std::vector<std::shared_ptr<evt::NamedEvent>> subscribe_events;
    };

    struct RemoteSubscriber {
        NodeId socket_index{};
    };

    struct SendRoute {
        Label label{};
        size_t label_size{};
        std::vector<handle_uid> publishers;
        std::vector<RemoteSubscriber> remote_subscribers;
        std::optional<LocalSubscriber> local_subscribers;
    };

    struct LocalPublisher {
        Label shm_block{};
        std::shared_ptr<evt::NamedEvent> publish_event;
    };

    struct RemotePublisher {
        NodeId socket;
    };

    struct RecvRoute {
        Label label{};
        size_t label_size{};
        std::vector<handle_uid> subscribers;
        std::variant<std::monostate, RemotePublisher, LocalPublisher> publisher;
    };


    class RouteTable {
        private:
            // all labels we send and recv and their data routes stored here
            // SendRoute and RecvRoute store indexes into TransportRegistry
            std::unordered_map<Label, SendRoute> m_send_routes;
            std::unordered_map<Label, RecvRoute> m_recv_routes;

        public:
            std::array<LabelInfo, MAX_LABELS> get_send_labels() const;
            std::array<LabelInfo, MAX_LABELS> get_recv_labels() const;

        private:
            void create_send_route(Label label, SendHandle* handle);
            void create_recv_route(Label label, RecvHandle* handle);

            SendRoute* require_send_route(Label label, const char* fn);
            RecvRoute* require_recv_route(Label label, const char* fn);
            bool require_route_size(size_t expected, size_t actual, const char* fn);
            
        public:
            // send route ops
            bool add_send_publisher(Label label, SendHandle* handle);
            bool remove_send_publisher(Label label, handle_uid uid);

            bool add_local_send_subscriber(Label label, size_t size, NodeId my_id, NodeId to_id);
            bool remove_local_send_subscriber(Label label, NodeId to_id);

            bool add_remote_send_subscriber(Label label, size_t size, NodeId to_id);
            bool remove_remote_send_subscriber(Label label, NodeId to_id);

            const SendRoute* get_send_route(Label label) const noexcept;
            SendRoute* get_send_route(Label label) noexcept;

            // send route query
            bool has_send_route(Label label) const noexcept;
            bool is_send_publisher(Label label, handle_uid uid) const noexcept;
            bool is_local_send_subscriber(Label label, NodeId to_id) const noexcept;
            bool is_remote_send_subscriber(Label label, NodeId to_id) const noexcept;

            // recv route ops
            bool add_recv_subscriber(Label label, RecvHandle* handle);
            bool remove_recv_subscriber(Label label, handle_uid uid);

            bool add_local_recv_publisher(Label label, size_t size, NodeId my_id, NodeId from_id);
            bool remove_local_recv_publisher(Label label, NodeId from_id);

            bool add_remote_recv_publisher(Label label, size_t size, NodeId from_id);
            bool remove_remote_recv_publisher(Label label, NodeId from_id);

            const RecvRoute* get_recv_route(Label label) const noexcept;
            RecvRoute* get_recv_route(Label label) noexcept;

            // recv route query
            bool has_recv_route(Label label) const noexcept;
            bool is_recv_subscriber(Label label, handle_uid id) const noexcept;
            bool is_local_recv_publisher(Label label, NodeId from_id) const noexcept;
            bool is_remote_recv_publisher(Label label, NodeId from_id) const noexcept;

            std::vector<handle_uid>
            snapshot_send_publishers(Label label) const;

            std::vector<handle_uid>
            snapshot_recv_subscribers(Label label) const;
    };
}
