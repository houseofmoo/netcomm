#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

#include "types/types.h"
#include "route_table.h"
#include "transport_registry.h"
#include "dispatcher.h"

namespace eroil {
    class Router {
        private:
            mutable std::shared_mutex m_router_mtx;

            RouteTable m_routes;
            TransportRegistry m_transports;
            Dispatcher m_dispatch;

            std::unordered_map<handle_uid, std::unique_ptr<SendHandle>> m_send_handles;
            std::unordered_map<handle_uid, std::unique_ptr<RecvHandle>> m_recv_handles;

        public:
            Router();
            ~Router();

            // open/close send/recv
            void register_send_publisher(std::unique_ptr<SendHandle> handle);
            void unregister_send_publisher(SendHandle* handle);
            void register_recv_subscriber(std::unique_ptr<RecvHandle> handle);
            void unregister_recv_subscriber(RecvHandle* handle);

            // route interface
            void add_local_send_subscriber(Label label, size_t label_size, NodeId my_id, NodeId to_id);
            void add_remote_send_subscriber(Label label, size_t label_size, NodeId to_id);
            void add_local_recv_publisher(Label label, size_t label_size, NodeId my_id, NodeId from_id);
            void add_remote_recv_publisher(Label label, size_t label_size, NodeId from_id);

            void remove_local_send_subscriber(Label label, NodeId to_id);
            void remove_remote_send_subscriber(Label label, NodeId to_id);
            void remove_local_recv_publisher(Label label, NodeId my_id);
            void remove_remote_recv_publisher(Label label, NodeId from_id);

            // for broadcasts checks
            std::array<LabelInfo, MAX_LABELS> get_send_labels() const;
            std::array<LabelInfo, MAX_LABELS> get_recv_labels() const;
            bool has_send_route(Label label) const;
            bool has_recv_route(Label label) const;
            bool is_send_subscriber(Label label, NodeId to_id) const;
            bool is_recv_publisher(Label label, NodeId from_id) const;

            // for recv workers
            bool upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock);
            std::shared_ptr<sock::TCPClient> get_socket(NodeId id);
            std::vector<std::shared_ptr<sock::TCPClient>> get_all_sockets();
            bool has_socket(NodeId id) const;
            std::shared_ptr<shm::Shm> get_send_shm(Label label);
            std::shared_ptr<shm::Shm> get_recv_shm(Label label);
            RecvRoute* get_recv_route(Label label);

            SendResult send_to_subscribers(Label label, const void* buf, size_t size);
            void recv_from_publisher(Label label, const void* buf, size_t size);
    };
}
