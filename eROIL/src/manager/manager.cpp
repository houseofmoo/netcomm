#include "manager.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <eROIL/print.h>

namespace eroil {
    static int unique_id() {
        static std::atomic<int> next_id{0};
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    Manager::Manager(int id, std::vector<NodeInfo> nodes) 
        : m_id(id), m_address_book{}, m_router{}, m_broadcast{} {

        // confirm we know who the fuck we are
        if (static_cast<size_t>(m_id) > nodes.size()) {
            ERR_PRINT("manager has no entry in nodes info list, manager cannot initialize");
            return;
        }

        m_address_book.insert_addresses(nodes[m_id], nodes);
        m_broadcast.setup(8080);
    }

    SendHandle* Manager::open_send(OpenSendData data) {
        auto handle = std::make_unique<SendHandle>(unique_id(), data);
        SendHandle* raw_handle = handle.get();
        m_router.register_send_publisher(std::move(handle));
        return raw_handle;
    }

    void Manager::send_label(SendHandle* handle, void* buf, size_t buf_size, size_t buf_offset) {
        //int label = handle->data.label;

        // figure out which buffer to use
        uint8_t* send_buf = nullptr;
        size_t send_size = 0;
        size_t send_offset = 0;
        if (buf != nullptr) {
            send_buf = reinterpret_cast<uint8_t*>(buf);
            send_size = buf_size;
            send_offset = buf_offset;
        } else {
            send_buf = handle->data.buf;
            send_size = handle->data.buf_size;
            send_offset = handle->data.buf_offset;
        }

        // copy data into a local buffer so sender does not have to wait for send to complete
        auto data = std::make_unique<uint8_t[]>(send_size);
        std::memcpy(data.get(), send_buf + send_offset, send_size);

        // give data to a send thread that will call m_router.send(data, send_size)
    }

    void Manager::close_send(SendHandle* handle) {
        m_router.unregister_send_publisher(handle);
    }

    RecvHandle* Manager::open_recv(OpenReceiveData data) {
        auto handle = std::make_unique<RecvHandle>(unique_id(), data);
        RecvHandle* raw_handle = handle.get();
        m_router.register_recv_subscriber(std::move(handle));
        return raw_handle;
    }

    void Manager::close_recv(RecvHandle* handle) {
        m_router.unregister_recv_subscriber(handle);
    }
    
    void Manager::send_broadcast() {
        BroadcastMessage msg;
        msg.id = m_id;
        msg.send_labels.fill(LabelInfo{ -1, 0 });
        msg.recv_labels.fill(LabelInfo{ -1, 0 });

        uint32_t count = 0;
        auto send_labels = m_router.get_send_labels();
        for (const auto& [label, size] : send_labels) {
            msg.send_labels[count].label = label;
            msg.send_labels[count].size = size;
            count += 1;
            if (count >= MAX_LABELS) {
                ERR_PRINT("too many send labels");
                break;
            }
        }
        
        count = 0;
        auto recv_labels = m_router.get_recv_labels();
        for (const auto& [label, size] : recv_labels) {
            msg.recv_labels[count].label = label;
            msg.recv_labels[count].size = size;
            count += 1;
            if (count >= MAX_LABELS) {
                ERR_PRINT("too many recv labels");
                break;
            }
        }

        m_broadcast.send(msg);
    }

    void Manager::recv_broadcast() {
        BroadcastMessage msg;
        m_broadcast.recv(msg);
        if (msg.id == m_id) return; // we got a broadcast from ourselves -.-
        
        // check if they want a label we send
        for (const auto& info : msg.recv_labels) {
            if (info.label == -1) continue; // invalid label

            // if we do not send this label, ignore it
            if (!m_router.has_send_label(info.label)) continue;

            // check if they're already on subscriber list
            if (m_router.is_send_subscriber(info.label, msg.id)) continue;

            // this is a label they want to recv, but are not on the subscription list
            const auto addr = m_address_book.get(msg.id);
            switch (addr.kind) {
                case RouteKind::Shm: {
                    m_router.add_local_send_subscriber(info.label, info.size, msg.id);
                    break;
                }
                case RouteKind::Socket: {
                    m_router.add_remote_send_subscriber(info.label, info.size, msg.id);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add send subscriber but did not know routekind");
                    break;
                }
            }
        }

        // check if we want a label they send
        for (const auto& info : msg.send_labels) {
            if (info.label == -1) continue; // invalid label

            // if we do not want this label, ignore it
            if (!m_router.has_recv_label(info.label)) continue;

            // check if we're already subscribed
            if (m_router.is_recv_publisher(info.label, msg.id, m_id)) continue;

            // this is a label we want to recv and have not registered a publisher
            const auto addr = m_address_book.get(msg.id);
            switch (addr.kind) {
                case RouteKind::Shm: {
                    m_router.set_local_recv_publisher(info.label, info.size, m_id);
                    // for each new local recv publisher, we need a new thread to listen to for
                    // that label
                    break;
                }
                case RouteKind::Socket: {
                    m_router.set_remote_recv_publisher(info.label, info.size, msg.id);
                    // should already have 1 thread per socket connection that listens for data from that peer
                    // that thread recv's data, then passes it to the router 
                    // m_router.recv_from_publisher(Label label, const void* buf, size_t size);
                    break;
                }
                default: {
                    ERR_PRINT("tried to add send subscriber but did not know routekind");
                    break;
                }
            }
        }
    }
}