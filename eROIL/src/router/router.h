#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <shared_mutex>
#include <optional>

#include "types/types.h"
#include "types/handles.h"
#include "net/socket.h"
#include "shm/shm.h"

namespace eroil {
    enum class SendOpErr {
        None,
        RouteNotFound,
        SizeMismatch,
        ShmMissing,
        SocketMissing,
        Failed,
    };

    struct SocketConn {
        NodeId to_id;
        std::string ip;
        uint16_t port;
        std::shared_ptr<net::ClientSocket> socket;
    };

    struct SendRoute {
        Label label;
        size_t label_size;
        
        std::vector<handle_uid> publishers;     // index of handles that send this label
        std::vector<NodeId> remote_subscribers; // outgoing remote subscribers of this label
        // local_subscribers is held in ShmSend index by label
    };

    struct RecvRoute {
        Label label;
        size_t label_size;

        // recv is special, we either recv is over shm OR socket, cannot be both
        std::vector<handle_uid> subscribers;    // index of handles that want this label
        std::optional<NodeId> remote_publisher; // incoming remote publisher of this label
        // local_publishers is held in ShmRecv index by label

        std::unique_ptr<uint8_t[]> scratch_buf; // store received data here temporarily
    };

    struct SendSnapshot {
        std::shared_ptr<shm::ShmSend> shm = nullptr;
        std::vector<std::shared_ptr<net::ClientSocket>> sockets;
        bool has_local = false;
        bool has_remote = false;
    };

    class Router {
        private:
            // when mutating router, lock it down
            mutable std::shared_mutex m_router_mtx;
            
            // communication handles
            std::unordered_map<NodeId, SocketConn> m_sockets; // remove send/recv
            std::unordered_map<Label, std::shared_ptr<shm::ShmSend>> m_send_shm; // local send
            std::unordered_map<Label, std::shared_ptr<shm::ShmRecv>> m_recv_shm; // local recv

            // send and recv routes per label
            std::unordered_map<Label, SendRoute> m_send_routes;
            std::unordered_map<Label, RecvRoute> m_recv_routes;

            // all handles that are currently active
            std::unordered_map<handle_uid, std::unique_ptr<SendHandle>> m_send_handles;
            std::unordered_map<handle_uid, std::unique_ptr<RecvHandle>> m_recv_handles;

        public:
            Router();
            ~Router();

            void register_send_publisher(std::unique_ptr<SendHandle> handle);
            void unregister_send_publisher(SendHandle* handle);

            void register_recv_subscriber(std::unique_ptr<RecvHandle> handle);
            void unregister_recv_subscriber(RecvHandle* handle);

            void add_local_send_subscriber(const Label label, const size_t size, const NodeId to_id);
            void add_remote_send_subscriber(const Label label, const size_t size, const NodeId to_id);

            void set_local_recv_publisher(const Label label, const size_t size, const NodeId my_id);
            void set_remote_recv_publisher(const Label label, const size_t size, const NodeId from_id);

            SendOpErr send_to_subscribers(const Label label, const void* buf, const size_t size);
            void recv_from_publisher(const Label label, const void* buf, const size_t size);

            SendHandle* get_send_handle(handle_uid uid) const;
            RecvHandle* get_recv_handle(handle_uid uid) const;

            std::vector<std::pair<Label, size_t>> get_send_labels() const;
            std::vector<std::pair<Label, size_t>> get_recv_labels() const;
            bool has_send_label(const Label label) const;
            bool has_recv_label(const Label label) const;

            bool is_send_subscriber(const Label label, const NodeId id) const;
            bool is_recv_publisher(const Label label, const NodeId from_id, const NodeId my_id) const;

        private:
            // REQUIRES m_router_mtx held when called
            SendSnapshot snapshot_nolock(Label label) const;
            // LOCKS mtx then returns the snapshot
            SendSnapshot snapshot_locked(Label label);
    };
}
