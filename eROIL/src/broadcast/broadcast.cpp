#include "broadcast.h"
#include <thread>
#include <chrono>
#include <memory>
#include <eROIL/print.h>

namespace eroil {
    Broadcast::Broadcast(NodeId id, Address& address, Router& router) :
        m_id(id), 
        m_address_book(address),
        m_router(router), 
        m_udp{},
        m_port{0} {}

    Broadcast::~Broadcast() {
        m_udp.close();
    }

    bool Broadcast::start_broadcast(uint16_t port) {
        m_port = port;
        sock::UdpMcastConfig cfg; // default config other than port
        cfg.port = m_port;
        
        auto result = m_udp.open_and_join(cfg);
        if (result.code != sock::SockErr::None) {
            ERR_PRINT("udp multicast group join failed, broadcast exiting");
            print_socket_result(result);
            return false;
        }

        std::thread send_broadcast_thread([this]() {
            while (true) {
                send_broadcast();
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        });
        send_broadcast_thread.detach();

        std::thread recv_broadcast_thread([this]() {
            while (true) { recv_broadcast(); }
        });
        recv_broadcast_thread.detach();
        
        return true;
    }

    bool Broadcast::is_connected() const {
        return m_udp.is_open();
    }
    
    void Broadcast::send_broadcast() {
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

        PRINT(m_id, " sends broadcast");
        m_udp.send_broadcast(&msg, sizeof(msg));
    }

    void Broadcast::recv_broadcast() {
        BroadcastMessage msg;
        m_udp.recv_broadcast(&msg, sizeof(msg));
        if (msg.id == m_id) return; // we got a broadcast from ourselves -.-
        
        PRINT(m_id, " recv broadcast from: ", msg.id);
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