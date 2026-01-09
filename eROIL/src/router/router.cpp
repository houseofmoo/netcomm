#include "router.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <eROIL/print.h>
#include "platform.h"
#include "utils.h"

namespace eroil {
    static void push_unique(std::vector<NodeId>& v, NodeId id) {
        if (std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
    }

    static void push_unique(std::vector<handle_uid>& v, handle_uid uid) {
        if (std::find(v.begin(), v.end(), uid) == v.end()) v.push_back(uid);
    }

    Router::Router() : m_sockets{}, m_send_shm{}, m_recv_shm{}, m_send_routes{}, m_recv_routes{} {}

    Router::~Router() = default;

    void Router::register_send_publisher(std::unique_ptr<SendHandle> handle) {
        std::unique_lock lock(m_router_mtx);
        
        const handle_uid uid = handle->uid;
        const Label label = handle->data.label;
        const size_t size = handle->data.buf_size;

        auto [handles_it, handle_inserted] = m_send_handles.try_emplace(
            uid,
            std::move(handle)
        );

        if (!handle_inserted) {
            ERR_PRINT("Attempted to register send handle, but it was a duplicate!");
            return;
        }

        // create the send route if it does not exist, otherwise just add the new publisher
        auto [routes_it, route_inserted] = m_send_routes.try_emplace(
            label,
            SendRoute{ label, size, {}, {} }
        );

        if (!route_inserted && routes_it->second.label_size != size) {
            ERR_PRINT("Size mismatch when registering a new send publisher");
            m_send_handles.erase(uid);
            return;
        }

        push_unique(routes_it->second.publishers, uid);
    }

    void Router::unregister_send_publisher(SendHandle* handle) {
        if (handle == nullptr) return;
        std::unique_lock lock(m_router_mtx);

        // confirm pointer is valid
        auto handle_it = m_send_handles.find(handle->uid);
        if (handle_it == m_send_handles.end() || handle_it->second.get() != handle) {
            ERR_PRINT("Attempted to unregister a invalid Sendhandle");
            return;
        }

        const auto label = handle->data.label;
        const auto uid = handle->uid;

        // delete handle
        m_send_handles.erase(uid);

        // update route
        auto route_it = m_send_routes.find(label);
        if (route_it == m_send_routes.end()) {
            ERR_PRINT("Attempted to unregister send handle, but label had no route");
            return;
        }

        auto& publishers = route_it->second.publishers;
        publishers.erase(
            std::remove(publishers.begin(), publishers.end(), uid),
            publishers.end()
        );

        // delete route and any associated shm if route is unused
        if (publishers.empty()) {
            m_send_routes.erase(route_it);
            m_send_shm.erase(label);
            // sockets are shared, never close those
        }
    }

    void Router::register_recv_subscriber(std::unique_ptr<RecvHandle> handle) {
        std::unique_lock lock(m_router_mtx);
        
        const handle_uid uid = handle->uid;
        const Label label = handle->data.label;
        const size_t size = handle->data.buf_size;

        auto [handles_it, handle_inserted] = m_recv_handles.try_emplace(
            uid,
            std::move(handle)
        );

        if (!handle_inserted) {
            ERR_PRINT("Attempted to register recv handle, but it was a duplicate!");
            return;
        }

        // create the send route if it does not exist, otherwise just add the new publisher
        auto [routes_it, route_inserted] = m_recv_routes.try_emplace(
            label,
            RecvRoute{ label, size, {}, std::nullopt, std::make_unique<uint8_t[]>(size) }
        );

        if (!route_inserted && routes_it->second.label_size != size) {
            ERR_PRINT("Size mismatch when registering a new recv subscriber");
            m_recv_handles.erase(uid);
            return;
        }
     
        push_unique(routes_it->second.subscribers, uid);
    }

    void Router::unregister_recv_subscriber(RecvHandle* handle) {
        if (handle == nullptr) return;
        std::unique_lock lock(m_router_mtx);

        // confirm pointer is valid
        auto handle_it = m_recv_handles.find(handle->uid);
        if (handle_it == m_recv_handles.end() || handle_it->second.get() != handle){
            ERR_PRINT("Attempted to unregister an invalid RecvHandle");
            return;
        }
        
        const auto label = handle->data.label;
        const auto uid = handle->uid;

        // delete handle
        m_recv_handles.erase(uid);

        // update route
        auto route_it = m_recv_routes.find(label);
        if (route_it == m_recv_routes.end()) {
            ERR_PRINT("Attempted to unregister recv handle, but label had no route");
            return;
        }

        auto& subscribers = route_it->second.subscribers;
        subscribers.erase(
            std::remove(subscribers.begin(), subscribers.end(), uid),
            subscribers.end()
        );

        // delete route and any associated shm if route is unused
        if (subscribers.empty()) {
            m_recv_routes.erase(route_it);
            m_recv_shm.erase(label);
            // sockets are shared, never close those
        }
    }

    void Router::add_local_send_subscriber(const Label label, const size_t size, const NodeId to_id) {
        // lock router for mutation
        std::unique_lock lock(m_router_mtx);

        // if send route doesnt exist, insert it
        auto [routes_it, inserted] = m_send_routes.try_emplace(
            label,
            SendRoute{ label, 0, {}, {} }
        );

        // confirm size matches expected
        if (!inserted && routes_it->second.label_size != size) {
            ERR_PRINT("Tried to add local send subscriber but size did not match expected size");
            return;
        }
        
        // if shm memory block exists, just add new subscriber
        auto send_shm_it = m_send_shm.find(label);
        if (send_shm_it != m_send_shm.end()) {
            auto shm_evt_err = send_shm_it->second->add_send_event(to_id);
            if (shm_evt_err != shm::ShmOpErr::None) {
                ERR_PRINT("Error adding send event to shared memory block for label: ", label);
            }
            return; 
        }
   
        // create a shared memory block for this label
        auto shm = std::make_shared<shm::ShmSend>(label, size);

        // open block (5 tries)
        auto shm_open_err = shm->open_new_rety(5, 100);
        if (shm_open_err != shm::ShmErr::None) {
            ERR_PRINT("Error opening new send shared memory block for label: ", label);
            return;
        }

        // add recvr for event
        auto shm_evt_err = shm->add_send_event(to_id);
        if (shm_evt_err != shm::ShmOpErr::None) {
            ERR_PRINT("Error adding send event to new shared memory block for label: ", label);
            return;
        }
        
        // store shared memory send
        m_send_shm.emplace(label, shm);
    }

    void Router::add_remote_send_subscriber(const Label label, const size_t size, const NodeId to_id) {
        // lock router for mutation
        std::unique_lock lock(m_router_mtx);

        // socket should always already exist
        auto socket_it = m_sockets.find(to_id);
        if (socket_it == m_sockets.end()) {
            ERR_PRINT("Could not find socket to NodeId: ", to_id);
            return;
        }

        // if send route doesnt exist, insert it
        auto [routes_it, inserted] = m_send_routes.try_emplace(
            label,
            SendRoute{ label, size, {}, {} }
        );

        // confirm size matches expected
        if (!inserted && routes_it->second.label_size != size) {
            ERR_PRINT("Tried to add remote send subscriber but size did not match expected size");
            return;
        }

        // add remote subscriber
        push_unique(routes_it->second.remote_subscribers, to_id);
    }

    void Router::set_local_recv_publisher(const Label label, const size_t size, const NodeId my_id) {
        // lock router for mutation
        std::unique_lock lock(m_router_mtx);

        // if recv route doesnt exist, insert it
        auto [routes_it, inserted] = m_recv_routes.try_emplace(
            label,
            RecvRoute{ label, size, {}, std::nullopt, std::make_unique<uint8_t[]>(size) }
        );

        // confirm size matches expected
        if (!inserted && routes_it->second.label_size != size) {
            ERR_PRINT("Tried to add local recv publisher but size did not match expected size");
            return;
        }

        // check if a remote publisher is set, if so overwrite it with local since that is higher priority
        if (routes_it->second.remote_publisher.has_value()) {
            routes_it->second.remote_publisher = std::nullopt;
        }
        
        // check if we need to create shared memory block
        auto recv_shm_it = m_recv_shm.find(label);
        if (recv_shm_it != m_recv_shm.end()) return;

        // create a shared memory block for this label
        auto shm = std::make_shared<shm::ShmRecv>(label, size);

        // open block (5 tries)
        auto shm_open_err = shm->open_existing_rety(5, 100);
        if (shm_open_err != shm::ShmErr::None) {
            ERR_PRINT("Error opening new recv shared memory block for label: ", label);
            return;
        }

        // set event
        auto shm_evt_err = shm->set_recv_event(my_id);
        if (shm_evt_err != shm::ShmOpErr::None) {
            ERR_PRINT("Error adding send event to new shared memory block for label: ", label);
            return;
        }
        
        // store shared memory send
        m_recv_shm.emplace(label, shm);
    }

    void Router::set_remote_recv_publisher(const Label label, const size_t size, const NodeId from_id) {
        // lock router for mutation
        std::unique_lock lock(m_router_mtx);

        // check if a local recv block exists, if so do not add remote
        if (m_recv_shm.find(label) != m_recv_shm.end()) {
            return;
        }

        // socket should always already exist
        auto socket_it = m_sockets.find(from_id);
        if (socket_it == m_sockets.end()) {
            ERR_PRINT("Could not find socket to NodeId: ", from_id);
            return;
        }

        // if recv route doesnt exist, insert it
        auto [routes_it, inserted] = m_recv_routes.try_emplace(
            label,
            RecvRoute{ label, size, {}, std::nullopt, std::make_unique<uint8_t[]>(size)}
        );

        // confirm size matches expected
        if (!inserted && routes_it->second.label_size != size) {
            ERR_PRINT("Tried to add remote recv publisher but size did not match expected size");
            return;
        }

        // set remote route
        if (routes_it->second.remote_publisher.has_value()) {
            if (*routes_it->second.remote_publisher == from_id) return;
            ERR_PRINT("Attempted to overwrite existing socket for label: ", label);
            return;
        }
        routes_it->second.remote_publisher = from_id;
    }

    SendOpErr Router::send_to_subscribers(const Label label, const void* buf, const size_t size) {
        // call this to send data to all subscribers
        SendSnapshot snapshot;
        {
            std::shared_lock lock(m_router_mtx);

            auto send_route_it = m_send_routes.find(label);
            if (send_route_it == m_send_routes.end()) {
                ERR_PRINT("Called send on a label that has no send route");
                return SendOpErr::RouteNotFound;
            }

            // optional: size check here while locked
            if (size != send_route_it->second.label_size) {
                ERR_PRINT("Size mismatch for label: ", label);
                return SendOpErr::SizeMismatch;
            }

            // if no publishers of this label, what is going on?
            if (send_route_it->second.publishers.size() <= 0) {
                ERR_PRINT("Send called on a label with no publishers, label: ", label);
                return SendOpErr::Failed;
            }

            snapshot = snapshot_nolock(label);
            if (!snapshot.has_local && !snapshot.has_remote) {
                return SendOpErr::None;
            }
        }

        bool failed = false;

        // send local
        if (snapshot.has_local) {
            if (snapshot.shm == nullptr) {
                failed = true;
            } else {
                auto err = snapshot.shm->send(buf, size);
                if (err != shm::ShmOpErr::None) failed = true;
            }
        }

        // send remote
        if (snapshot.has_remote) {
            for (auto& sock : snapshot.sockets) {
                if (sock == nullptr) {
                    failed = true;
                    continue;
                }
                sock->send(buf, size);
            }
        }

        // update send iosb here?

        return failed ? SendOpErr::Failed : SendOpErr::None;
    }

    void Router::recv_from_publisher(const Label label, const void* buf, const size_t size) {
        // call this after recving data to write to all subscribers

        if (buf == nullptr || size == 0) return;

        std::shared_lock lock(m_router_mtx);

        // route must exist
        auto route_it = m_recv_routes.find(label);
        if (route_it == m_recv_routes.end()) {
            ERR_PRINT("recv() called for unknown label: ", label);
            return;
        }

        RecvRoute& route = route_it->second;

        if (size != route.label_size) {
            ERR_PRINT("recv() size mismatch for label: ", label, " expected=", route.label_size, " got=", size);
            return;
        }

        if (route.subscribers.empty()) {
            return;
        }

        for (auto uid : route.subscribers) {
            auto handle_it = m_recv_handles.find(uid);
            if (handle_it == m_recv_handles.end() || handle_it->second == nullptr) {
                ERR_PRINT("recv() found stale recv handle");
                continue; // stale uid (should never occur)
            }

            RecvHandle* handle = handle_it->second.get();
            OpenReceiveData& data = handle->data;

            if (data.buf == nullptr || data.buf_slots == 0 || data.buf_size == 0) {
                ERR_PRINT("recv() handle had no where to copy data to");
                continue;
            }

            // copy data
            const size_t slot = (data.buf_index % data.buf_slots);
            uint8_t* dst = data.buf + (slot * data.buf_size);
            std::memcpy(dst, buf, size);

            // move buf index forward (ring)
            data.buf_index = (slot + 1) & data.buf_slots;

            // iosb update
            if (data.iosb != nullptr && data.num_iosb > 0) {
                const size_t iosb_slot = (data.iosb_index % data.num_iosb);

                // UPDATE IOSB

                data.iosb_index = (iosb_slot + 1) % data.num_iosb;
            }

            signal_recv_sem(data.sem, data.signal_mode);
        }
    }

    SendHandle* Router::get_send_handle(handle_uid uid) const {
        std::shared_lock lock(m_router_mtx);
        auto it = m_send_handles.find(uid);
        if (it == m_send_handles.end()) return nullptr;
        return it->second.get();
    }

    RecvHandle* Router::get_recv_handle(handle_uid uid) const {
        std::shared_lock lock(m_router_mtx);
        auto it = m_recv_handles.find(uid);
        if (it == m_recv_handles.end()) return nullptr;
        return it->second.get();
    }

    std::vector<std::pair<Label, size_t>> Router::get_send_labels() const {
        std::shared_lock lock(m_router_mtx);

        std::vector<std::pair<Label, size_t>> labels;
        labels.reserve(m_send_routes.size());

        for (const auto& [label, route] : m_send_routes) {
            labels.emplace_back(std::pair(label, route.label_size));
        }

        return labels;
    }

    std::vector<std::pair<Label, size_t>> Router::get_recv_labels() const {
        std::shared_lock lock(m_router_mtx);

        std::vector<std::pair<Label, size_t>> labels;
        labels.reserve(m_recv_routes.size());

        for (const auto& [label, route] : m_recv_routes) {
            labels.emplace_back(std::pair(label, route.label_size));
        }

        return labels;
    }

    bool Router::has_send_label(const Label label) const {
        std::shared_lock lock(m_router_mtx);
        return (m_send_routes.find(label) != m_send_routes.end());
    }

    bool Router::has_recv_label(const Label label) const {
        std::shared_lock lock(m_router_mtx);
        return (m_recv_routes.find(label) != m_recv_routes.end());
    }

    bool Router::is_send_subscriber(const Label label, const NodeId id) const {
        std::shared_lock lock(m_router_mtx);

        auto route_it = m_send_routes.find(label);
        if (route_it == m_send_routes.end()) {
            ERR_PRINT("Checked if NodeId: ", id, " was a send subscriber, but route did not exist");
            return false; // this is a EVERYTHING IS BROKEN moment
        }

        // see if id lives in remote subs
        if (!route_it->second.remote_subscribers.empty()) {
            auto find_it = std::find(
                route_it->second.remote_subscribers.begin(),
                route_it->second.remote_subscribers.end(),
                id
            );

            if (find_it != route_it->second.remote_subscribers.end()) {
                return true;
            }
        }

        // check if id lives in shm send notification list
        auto send_shm_it = m_send_shm.find(label);
        if (send_shm_it != m_send_shm.end()) {
            return send_shm_it->second->has_send_event(label, id);
        }
        
        return false; 
    }

    bool Router::is_recv_publisher(const Label label, const NodeId from_id, const NodeId my_id) const {
        std::shared_lock lock(m_router_mtx);

        auto route_it = m_recv_routes.find(label);
        if (route_it == m_recv_routes.end()) {
            ERR_PRINT("Checked if NodeId: ", from_id, " was a recv producer, but route did not exist");
            return false; // this is a EVERYTHING IS BROKEN moment
        }

        // check if id is remote publisher
        if (route_it->second.remote_publisher.has_value()) {
            return *route_it->second.remote_publisher == from_id;
        }

        // check if id lives in shm recv notification list
        auto recv_shm_it = m_recv_shm.find(label);
        if (recv_shm_it != m_recv_shm.end()) {
            return recv_shm_it->second->has_recv_event(label, my_id);
        }

        return false;
    }

    SendSnapshot Router::snapshot_nolock(Label label) const {
        // REQUIRES m_router_mtx held when called
        SendSnapshot snap{};

        auto route_it = m_send_routes.find(label);
        if (route_it == m_send_routes.end()) return snap;

        const SendRoute& r = route_it->second;

        // local: shm exists for this label?
        if (auto shm_it = m_send_shm.find(label); shm_it != m_send_shm.end()) {
            snap.shm = shm_it->second;
            snap.has_local = true;
        }

        // remote: resolve sockets for each NodeId subscriber
        if (!r.remote_subscribers.empty()) {
            snap.sockets.reserve(r.remote_subscribers.size());
            for (NodeId peer : r.remote_subscribers) {
                auto socket_it = m_sockets.find(peer);
                if (socket_it != m_sockets.end() && socket_it->second.socket) {
                    snap.sockets.push_back(socket_it->second.socket);
                }
            }
            snap.has_remote = !snap.sockets.empty();
        }
        return snap;
    }

    SendSnapshot Router::snapshot_locked(Label label) {
        std::shared_lock lock(m_router_mtx);
        return snapshot_nolock(label);
    }
}
