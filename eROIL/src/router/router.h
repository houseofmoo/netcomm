#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

#include "types/types.h"
#include "route_table.h"
#include "transport_registry.h"
#include "dispatch.h"

namespace eroil {
    class Router {
        private:
            mutable std::shared_mutex m_router_mtx;

            RouteTable m_routes;
            TransportRegistry m_transports;
            Dispatcher m_dispatch;

            std::unordered_map<handle_uid, std::unique_ptr<hndl::SendHandle>> m_send_handles;
            std::unordered_map<handle_uid, std::unique_ptr<hndl::RecvHandle>> m_recv_handles;

        public:
            Router();
            ~Router();

            // open/close send/recv
            void register_send_publisher(std::unique_ptr<hndl::SendHandle> handle);
            void unregister_send_publisher(const hndl::SendHandle* handle);
            void register_recv_subscriber(std::unique_ptr<hndl::RecvHandle> handle);
            void unregister_recv_subscriber(const hndl::RecvHandle* handle);

            // route interface
            void add_local_send_subscriber(Label label, size_t size, NodeId dst_id);
            void add_remote_send_subscriber(Label label, size_t size, NodeId dst_id);
            void remove_local_send_subscriber(Label label, NodeId to_id);
            void remove_remote_send_subscriber(Label label, NodeId to_id);

            io::LabelsSnapshot get_send_labels_snapshot() const;
            io::LabelsSnapshot get_recv_labels_snapshot() const;
            std::array<io::LabelInfo, MAX_LABELS> get_send_labels() const;
            std::array<io::LabelInfo, MAX_LABELS> get_recv_labels() const;
            std::array<io::LabelInfo, MAX_LABELS> get_send_labels_sorted() const;
            std::array<io::LabelInfo, MAX_LABELS> get_recv_labels_sorted() const;

            bool has_send_route(Label label) const noexcept;
            bool has_recv_route(Label label) const noexcept;
            bool is_send_subscriber(Label label, NodeId to_id) const noexcept;

            bool upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock);
            std::shared_ptr<sock::TCPClient> get_socket(NodeId id) const noexcept;
            bool has_socket(NodeId id) const noexcept;
            std::vector<std::shared_ptr<sock::TCPClient>> get_all_sockets() const;
            
            bool open_send_shm(NodeId dst_id);
            std::shared_ptr<shm::ShmSend> get_send_shm(NodeId dst_id) const noexcept;
            bool open_recv_shm(NodeId my_id);
            std::shared_ptr<shm::ShmRecv> get_recv_shm() const noexcept;

            SendResult send_to_subscribers(const NodeId my_id, const Label label, const handle_uid uid, io::SendBuf send_buf) const;
            void recv_from_publisher(const Label label, const std::byte* buf, const size_t size, const size_t recv_offset) const;

    };
}
