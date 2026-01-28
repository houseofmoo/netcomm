#pragma once
#include <unordered_map>
#include <vector>
#include <optional>
#include <cstddef>
#include <variant>
#include <atomic>

#include "events/named_event.h"
#include "types/const_types.h"
#include "types/handles.h"
#include "types/label_io_types.h"
#include "types/macros.h"

namespace eroil {
    struct SendRoute {
        Label label;
        size_t label_size;
        std::vector<handle_uid> publishers;
        std::vector<NodeId> remote_subscribers;
        std::vector<NodeId> local_subscribers;
    };

    struct RecvRoute {
        Label label;
        size_t label_size;
        std::vector<handle_uid> subscribers;
    };

    class RouteTable {
        private:
            std::unordered_map<Label, SendRoute> m_send_routes;
            std::unordered_map<Label, RecvRoute> m_recv_routes;

            std::atomic<uint64_t> m_send_gen{1};
            std::atomic<uint64_t> m_recv_gen{1};

            void create_send_route(Label label, hndl::SendHandle* handle);
            void create_recv_route(Label label, hndl::RecvHandle* handle);
            
        public:
            RouteTable() = default;
            ~RouteTable() = default;

            EROIL_NO_COPY(RouteTable)
            EROIL_NO_MOVE(RouteTable)

            io::LabelsSnapshot get_send_labels_snapshot() const;
            io::LabelsSnapshot get_recv_labels_snapshot() const;
            std::array<io::LabelInfo, MAX_LABELS> get_send_labels() const;
            std::array<io::LabelInfo, MAX_LABELS> get_recv_labels() const;
            std::array<io::LabelInfo, MAX_LABELS> get_send_labels_sorted() const;
            std::array<io::LabelInfo, MAX_LABELS> get_recv_labels_sorted() const;

            // send route ops
            bool add_send_publisher(Label label, hndl::SendHandle* handle);
            bool remove_send_publisher(Label label, handle_uid uid);

            bool add_local_send_subscriber(Label label, size_t size, NodeId dst_id);
            bool add_remote_send_subscriber(Label label, size_t size, NodeId dst_id);
            
            bool remove_local_send_subscriber(Label label, NodeId dst_id);
            bool remove_remote_send_subscriber(Label label, NodeId dst_id);

            const SendRoute* get_send_route(Label label) const noexcept;
            SendRoute* get_send_route(Label label) noexcept;

            bool has_send_route(Label label) const noexcept;
            bool is_send_publisher(Label label, handle_uid uid) const noexcept;
            bool is_local_send_subscriber(Label label, NodeId dst_id) const noexcept;
            bool is_remote_send_subscriber(Label label, NodeId dst_id) const noexcept;

            // recv route ops
            bool add_recv_subscriber(Label label, hndl::RecvHandle* handle);
            bool remove_recv_subscriber(Label label, handle_uid uid);

            const RecvRoute* get_recv_route(Label label) const noexcept;
            RecvRoute* get_recv_route(Label label) noexcept;

            bool has_recv_route(Label label) const noexcept;
            bool is_recv_subscriber(Label label, handle_uid id) const noexcept;

            std::vector<handle_uid>
            snapshot_send_publishers(Label label) const;

            std::vector<handle_uid>
            snapshot_recv_subscribers(Label label) const;
    };
}
