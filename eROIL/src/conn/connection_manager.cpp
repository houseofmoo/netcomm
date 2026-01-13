#include "connection_manager.h"
#include <thread>
#include <chrono>
#include "types/types.h"
#include <eROIL/print.h>

namespace eroil {
    static bool map_sock_failures(sock::SockErr err) {
        switch (err) {
            case sock::SockErr::None: return true; // successful ping send
            case sock::SockErr::ResourceExhausted: return true; // does not require a re-connect
            case sock::SockErr::WouldBlock: return true; // do not require a re-connect
            case sock::SockErr::SizeZero: return true; // indicates we sent something too small, not a disconnect
            case sock::SockErr::SizeTooLarge: return true; // indicates we sent something too large, not a disconnect
            default: return false; // all others indicate this socket is likely dead and needs reconnections
        }
    }

    ConnectionManager::ConnectionManager(NodeId id, Router& router) : 
        m_id(id),
        m_router(router), 
        m_tcp_server{},
        m_sender{router},
        m_sock_recvrs{}, 
        m_shm_recvrs{} {}

    void ConnectionManager::start() {
        m_sender.start();
        start_tcp_server();

        std::thread search([this]() { search_peers(); });
        search.detach();

        std::thread monitor([this]() { monitor_sockets(); });
        monitor.detach();
    }

    void ConnectionManager::send_label(handle_uid uid, Label label, size_t buf_size, std::unique_ptr<uint8_t[]> data) {
        m_sender.enqueue(eroil::worker::SendQEntry{
            uid,
            label,
            buf_size,
            std::move(data)
        });
    }

    void ConnectionManager::start_local_recv_worker(Label label, size_t label_size) {
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

    void ConnectionManager::start_remote_recv_worker(NodeId peer_id) {
        // if we find a worker that already exists, we cannot stop them unless we know
        // the socket has also been closed. The assumption is someone already did the clean up 
        // (closing socket, stopping worker and deleting worker) before calling this function. 
        // So finding an existing worker for the peer is a error state
        auto it = m_sock_recvrs.find(peer_id);
        if (it != m_sock_recvrs.end()) {
            ERR_PRINT("attempted replace an existing worker, this can cause a deadlock. no worker started to nodeid=", peer_id);
            return;
        }

        auto [w_it, _] = m_sock_recvrs.emplace(
            peer_id,
            std::make_unique<worker::SocketRecvWorker>(
                m_router, 
                m_id,
                peer_id
            )
        );
        w_it->second->start();
    }

    void ConnectionManager::start_tcp_server() {
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
                
                client.set_destination_id(hdr.source_id);
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

    void ConnectionManager::search_peers() {
        LOG("searching for peers...");
        // how many peers do we expect to connect to

        // TODO: need to connect to all peers we expect to be alive
        // this thread will live until all connections established
        
        int expected_peers = 0;
        for (const auto& [id, info] : addr::get_address_book()) {
            if (info.kind == addr::RouteKind::Shm) continue;
            if (info.kind == addr::RouteKind::Self) continue;
            if (id >= m_id) continue;
            expected_peers += 1;
        }

        int actual_peers = 0;
        while (actual_peers < expected_peers) {
            for (const auto& [id, info] : addr::get_address_book()) {
                if (info.kind == addr::RouteKind::Shm) continue;
                if (info.kind == addr::RouteKind::Self) continue;
                if (id >= m_id) continue;
                if (m_router.has_socket(id)) continue;

                LOG("attempt connection to id=", id, " ip=", info.ip, ":", info.port);
                sock::TCPClient client;
                auto result = client.open_and_connect(info.ip.c_str(), info.port);
                if (result.code != sock::SockErr::None) {
                    ERR_PRINT("connection to ", id, " failed");
                    //print_socket_result(result);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                // send them a notice of who we are
                send_id(&client);
                client.set_destination_id(id);

                // insert socket into transport registry
                m_router.upsert_socket(
                    id, 
                    std::make_shared<sock::TCPClient>(std::move(client))
                );
                start_remote_recv_worker(id);
                LOG("established tcp connection to node: ", id);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                actual_peers += 1;
            }

            // search for remaining peers again in 5 seconds
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }

        LOG("peer connection complete");
    }

    void ConnectionManager::monitor_sockets() {
        // every couple of seconds we need to check that the sockets are still alive
        // if theyre dead, stop recv worker and attempt re-connection
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 5));
            auto sockets = m_router.get_all_sockets();
            for (auto& sock : sockets) {

                if (sock->is_connected()) {
                    // we think the socket is connected, lets confirm that...
                    bool connected = send_ping(sock.get());
                    if (connected) continue;
                }

                // do disconnect logic, this wont do anything if already disconnected
                NodeId peer_id = sock->get_destination_id();
                sock->disconnect();
                LOG("found dead socket to nodeid=", peer_id);

                // find worker for this socket, which may not exist if they were deleted
                // during a previous monitoring sweep
                auto worker_it = m_sock_recvrs.find(peer_id);
                if (worker_it != m_sock_recvrs.end()) {
                    worker_it->second->stop();
                    m_sock_recvrs.erase(worker_it);
                }

                // if they'll reconnect to us dont attempt reconnect
                if (peer_id >= m_id) {
                    continue;
                };
            
                // try to reconnect socket
                auto addr = addr::get_address(sock->get_destination_id());
                LOG("attempt re-connect to id=", addr.id, " ip=", addr.ip, ":", addr.port);
                
                auto result = sock->open_and_connect(addr.ip.c_str(), addr.port);
                if (result.code != sock::SockErr::None) {
                    ERR_PRINT("connection to ", addr.id, " failed");
                    continue;
                }

                send_id(sock.get());
                LOG("re-established tcp connection to node: ", addr.id);
                start_remote_recv_worker(peer_id);
            }
        }   
    }

    bool ConnectionManager::send_id(sock::TCPClient* sock) {
        // send them a notice of who we are
        uint16_t flags = 0;
        set_flag(flags, LabelFlag::Connect);
        LabelHeader hdr{};
        hdr.magic = MAGIC_NUM;
        hdr.version = VERSION,
        hdr.source_id = m_id,
        hdr.flags = flags,
        hdr.label = 0,
        hdr.data_size = 0;

        auto err = sock->send_all(&hdr, sizeof(hdr));
        return map_sock_failures(err.code);
    }

    bool ConnectionManager::send_ping(sock::TCPClient* sock) {
        // send ping header
        uint16_t flags = 0;
        set_flag(flags, LabelFlag::Ping);
        LabelHeader hdr{};
        hdr.magic = MAGIC_NUM;
        hdr.version = VERSION,
        hdr.source_id = m_id,
        hdr.flags = flags,
        hdr.label = 0,
        hdr.data_size = 0;

        auto err = sock->send_all(&hdr, sizeof(hdr));
        return map_sock_failures(err.code);
    }
}