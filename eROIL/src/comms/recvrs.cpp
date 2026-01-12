#include "recvrs.h"
#include <thread>
#include <chrono>
#include "types/label_hdr.h"
#include <eROIL/print.h>

namespace eroil {
    Comms::Comms(NodeId id, Router& router) : 
        m_id(id),
        m_router(router), 
        m_tcp_server{},
        m_sender{router},
        m_sock_recvrs{}, 
        m_shm_recvrs{} {}

    void Comms::start() {
        m_sender.start();
        start_tcp_server();
        search_peers();
    }

    void Comms::send_label(handle_uid uid, Label label, size_t buf_size, std::unique_ptr<uint8_t[]> data) {
        m_sender.enqueue(eroil::worker::SendQEntry{
            uid,
            label,
            buf_size,
            std::move(data)
        });
    }

    void Comms::start_local_recv_worker(Label label, size_t label_size) {
        auto it = m_shm_recvrs.find(label);
        if (it != m_shm_recvrs.end()) {
            it->second->stop();
            m_shm_recvrs.erase(it);
        }

        auto [w_it, _] = m_shm_recvrs.emplace(
            label,
            std::make_unique<worker::ShmRecvWorker>(
                m_router, 
                label, 
                label_size
            )
        );
        w_it->second->start();
    }

    void Comms::start_remote_recv_worker(NodeId peer_id) {
        auto it = m_sock_recvrs.find(peer_id);
        if (it != m_sock_recvrs.end()) {
            LOG("stopping existing socket recv worker to replace with new sock recv worker for socket to nodeit=", peer_id);
            it->second->stop();
            m_sock_recvrs.erase(it);
        }

        auto [w_it, _] = m_sock_recvrs.emplace(
            peer_id,
            std::make_unique<worker::SocketRecvWorker>(
                m_router, 
                peer_id, 
                std::function<void()>([this, peer_id]{
                    reconnect(peer_id);
                })
            )
        );
        w_it->second->start();
    }

    void Comms::start_tcp_server() {
        LOG("tcp server listen start...");
        std::thread tcp_server_thread([this]() {
            auto info = addr::get_address(m_id);
            auto ts_result = m_tcp_server.open_and_listen(info.port);
            if (ts_result.code != sock::SockErr::None) {
                ERR_PRINT("tcp server open failed, exiting");
                print_socket_result(ts_result);
                return;
            }

            while (true) {
                auto [client, result] = m_tcp_server.accept();
                if (result.code != sock::SockErr::None) {
                    ERR_PRINT("tcp server accepted connection but there was an error");
                    print_socket_result(result);
                    continue;
                }

                // they connected to us, they'll send a follow up message
                LabelHeader hdr{};
                auto recv_result = client.recv_all(&hdr, sizeof(hdr));
                if (recv_result.code != sock::SockErr::None) {
                    ERR_PRINT("tcp server had an error recving client information");
                    print_socket_result(recv_result);
                    client.disconnect();
                    continue;
                }

                if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                    ERR_PRINT("tcp server recvd invalid header from client");
                    client.disconnect();
                    continue;
                }

                if (!has_flag(hdr.flags, LabelFlag::Connect)) {
                    ERR_PRINT("tcp server recvd invalid request type");
                    client.disconnect();
                    continue;
                }

                m_router.upsert_socket(
                    hdr.source_id,
                    std::make_shared<sock::TCPClient>(std::move(client))
                );
                start_remote_recv_worker(hdr.source_id);
                LOG("established tcp connection to node: ", hdr.source_id);
            }
        });
        tcp_server_thread.detach();
    }

    void Comms::search_peers() {
        LOG("searching for peers...");
        for (const auto& [id, info] : addr::get_address_book()) {
            if (info.kind == addr::RouteKind::Shm) continue;
            if (id >= m_id) continue;

            LOG("attempt connection to id=", id, " ip=", info.ip, ":", info.port);
            sock::TCPClient client;
            auto result = client.open_and_connect(info.ip.c_str(), info.port);
            if (result.code != sock::SockErr::None) {
                ERR_PRINT("connection to ", id, " failed");
                print_socket_result(result);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // send them a notice of who we are
            uint16_t flags = 0;
            set_flag(flags, LabelFlag::Connect);
            LabelHeader hdr {
                MAGIC_NUM,
                VERSION,
                m_id,
                flags,
                0,
                0
            };
            client.send(&hdr, sizeof(hdr));

            m_router.upsert_socket(
                id, 
                std::make_shared<sock::TCPClient>(std::move(client))
            );
            start_remote_recv_worker(id);
            LOG("established tcp connection to node: ", id);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        LOG("peer connection complete");
    }

    void Comms::reconnect(NodeId peer_id) {
        auto addr = addr::get_address(peer_id);
        while (true) {
            LOG("attempt connection to id=", peer_id, " ip=", addr.ip, ":", addr.port);
            sock::TCPClient client;

            auto result = client.open_and_connect(addr.ip.c_str(), addr.port);
            if (result.code != sock::SockErr::None) {
                ERR_PRINT("connection to ", peer_id, " failed");
                print_socket_result(result);
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                continue;
            }

            // send them a notice of who we are
            uint16_t flags = 0;
            set_flag(flags, LabelFlag::Connect);
            LabelHeader hdr {
                MAGIC_NUM,
                VERSION,
                m_id,
                flags,
                0,
                0
            };
            client.send(&hdr, sizeof(hdr));

            m_router.upsert_socket(
                peer_id, 
                std::make_shared<sock::TCPClient>(std::move(client))
            );
            start_remote_recv_worker(peer_id);
            LOG("re-established tcp connection to node: ", peer_id);
            break;
        }
    }
}