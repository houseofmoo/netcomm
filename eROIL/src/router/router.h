#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

#include "types/types.h"
#include "types/handles.h"

#include "route_table.h"
#include "transport_registry.h"
#include "dispatcher.h"

namespace eroil {
    struct RecvDelivery {
        Label label{};
        size_t size{};
        std::vector<std::shared_ptr<RecvTarget>> targets;
    };

    class Router {
        private:
            mutable std::shared_mutex m_router_mtx;

            RouteTable m_routes;
            TransportRegistry m_transports;
            Dispatcher m_dispatch;

            std::unordered_map<handle_uid, std::unique_ptr<SendHandle>> m_send_handles;
            std::unordered_map<handle_uid, std::unique_ptr<RecvHandle>> m_recv_handles;

            std::unordered_map<handle_uid, std::shared_ptr<RecvTarget>> m_recv_targets;

        public:
            Router();
            ~Router();

            void register_send_publisher(std::unique_ptr<SendHandle> handle);
            void unregister_send_publisher(SendHandle* handle);

            void register_recv_subscriber(std::unique_ptr<RecvHandle> handle);
            void unregister_recv_subscriber(RecvHandle* handle);

            void add_local_send_subscriber(Label label, size_t size, NodeId to_id);
            void add_remote_send_subscriber(Label label, size_t size, NodeId to_id);

            void set_local_recv_publisher(Label label, size_t size, NodeId my_id);
            void set_remote_recv_publisher(Label label, size_t size, NodeId from_id);

            SendOpErr send_to_subscribers(Label label, const void* buf, size_t size);
            void recv_from_publisher(Label label, const void* buf, size_t size);

            SendHandle* get_send_handle(handle_uid uid) const;
            RecvHandle* get_recv_handle(handle_uid uid) const;

            std::vector<std::pair<Label, size_t>> get_send_labels() const;
            std::vector<std::pair<Label, size_t>> get_recv_labels() const;

            bool has_send_label(Label label) const;
            bool has_recv_label(Label label) const;

            bool is_send_subscriber(Label label, NodeId id) const;
            bool is_recv_publisher(Label label, NodeId from_id, NodeId my_id) const;

        private:
            std::vector<std::shared_ptr<RecvTarget>> 
            snapshot_recv_delivery(const std::vector<handle_uid>& uids) const;

    };
}
