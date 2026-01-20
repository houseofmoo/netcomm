#include "connection_manager.h"
#include <thread>
#include <chrono>
#include <eROIL/print.h>
#include "log/evtlog_api.h"

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

        // tcp server listener thread
        std::thread([this]() {
            run_tcp_server();
        }).detach();


        std::vector<addr::NodeAddress> remote_peers;
        for (const auto& [id, info] : addr::get_address_book()) {
            if (info.kind == addr::RouteKind::Shm) continue;
            if (info.kind == addr::RouteKind::Self) continue;
            if (id >= m_id) continue;
            remote_peers.push_back(info);
        }

        const int max_attempts = 5;
        int attempts = 0;
        const int expected_peers = remote_peers.size();
        int connected_peers = 0;
        
        // try to find remote peers to connect to N times before moving on
        while (attempts < max_attempts) {
            for (const auto& info : remote_peers) {
                auto socket = m_router.get_socket(info.id);
                if (socket != nullptr) continue;
                
                if (connect_to_remote_peer(info)) {
                    connected_peers += 1;
                }
            }

            // if we found all peers we expected to cnnect to, we're done
            if (connected_peers >= expected_peers) {
                LOG("connected to ", connected_peers, " out of ", expected_peers, " expected peers");
                break;
            }

            // only sleep if we're going to try again
            if (attempts + 1 < max_attempts) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
            }
        }

        // start monitor thread
        std::thread([this]() { 
            remote_connection_monitor(); 
        }).detach();
    }

    void ConnectionManager::send_label(handle_uid uid, Label label, SendBuf send_buf) {
        m_sender.enqueue(worker::SendQEntry{
            uid,
            label,
            std::move(send_buf)
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

    void ConnectionManager::stop_local_recv_worker(Label label) {
        auto it = m_shm_recvrs.find(label);
        if (it != m_shm_recvrs.end()) {
            // this call has a thread.join(), if we see a "blocks forever" this may be the cause
            it->second->stop();
            m_shm_recvrs.erase(it);
        }
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

    void ConnectionManager::stop_remote_recv_worker(NodeId from_id) {
        auto it = m_sock_recvrs.find(from_id);
        if (it != m_sock_recvrs.end()) {
            // this call has a thread.join(), if we see a "blocks forever" this may be the cause
            it->second->stop();
            m_sock_recvrs.erase(it);
        }
    }

    void ConnectionManager::run_tcp_server() {
        auto info = addr::get_address(m_id);
        LOG("tcp server listen start at ", info.ip, ":", info.port);
        evtlog::info(elog_kind::TCPServer_Start, elog_cat::TCPServer, info.port);

        auto ts_result = m_tcp_server.open_and_listen(info.port);
        if (ts_result.code != sock::SockErr::None) {
            ERR_PRINT("tcp server open failed, exiting");
            evtlog::crit(elog_kind::StartFailed, elog_cat::TCPServer);
            print_socket_result(ts_result);
            return;
        }

        while (true) {
            auto [client, result] = m_tcp_server.accept();
            if (result.code != sock::SockErr::None ||
                client == nullptr) {
                ERR_PRINT("tcp server accepted connection but there was an error");
                evtlog::warn(elog_kind::AcceptFailed, elog_cat::TCPServer);
                print_socket_result(result);
                continue;
            }

            // they connected to us, they'll send a follow up message
            LabelHeader hdr{};
            auto recv_result = client->recv_all(&hdr, sizeof(hdr));
            if (recv_result.code != sock::SockErr::None) {
                ERR_PRINT("tcp server had an error recving client information");
                evtlog::warn(elog_kind::ConnectionFailed, elog_cat::TCPServer);
                print_socket_result(recv_result);
                client->disconnect();
                continue;
            }

            if (hdr.magic != MAGIC_NUM || hdr.version != VERSION) {
                ERR_PRINT("tcp server recvd invalid header from client");
                evtlog::warn(elog_kind::InvalidHeader, elog_cat::TCPServer);
                client->disconnect();
                continue;
            }

            if (!has_flag(hdr.flags, LabelFlag::Connect)) {
                ERR_PRINT("tcp server recvd invalid request type");
                evtlog::warn(elog_kind::InvalidHeader, elog_cat::TCPServer, hdr.source_id);
                client->disconnect();
                continue;
            }
            
            client->set_destination_id(hdr.source_id);
            m_router.upsert_socket(
                hdr.source_id,
                std::move(client)
            );
            start_remote_recv_worker(hdr.source_id);
            LOG("established tcp connection to node: ", hdr.source_id);
            evtlog::info(elog_kind::NewConnection, elog_cat::TCPServer, hdr.source_id);
        }
    }

    void ConnectionManager::remote_connection_monitor() {
        LOG("remote peers to monitor thread starts");
        std::vector<addr::NodeAddress> remote_peers;
        for (const auto& [id, info] : addr::get_address_book()) {
            if (info.kind == addr::RouteKind::Shm) continue;
            if (info.kind == addr::RouteKind::Self) continue;
            remote_peers.push_back(info);
        }

        if (remote_peers.empty()) {
            LOG("no remote peers to monitor, remote peer monitor thread exits");
            return;
        }

        while (true) {
            for (const auto& info : remote_peers) {
                auto socket = m_router.get_socket(info.id);
                if (socket == nullptr) {
                    // if connection does not exist, connect
                    connect_to_remote_peer(info);
                } else {
                    // otherwise check socket connected
                    ping_remote_peer(info, socket);
                }
            }
            
            // sleep for 5 seconds
            std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
        }
    }

    bool ConnectionManager::connect_to_remote_peer(addr::NodeAddress peer_info) {
        // do not attempt connection to ID's greater than ours, they'll connect to us
        if (peer_info.id >= m_id) return false;
        
        evtlog::info(elog_kind::Connect_Start, elog_cat::SocketMonitor, peer_info.id);

        // attempt connection
        LOG("attempt connection to nodeid=", peer_info.id, " ip=", peer_info.ip, ":", peer_info.port);
        auto client = std::make_shared<sock::TCPClient>();
        auto result = client->open_and_connect(peer_info.ip.c_str(), peer_info.port);
        if (result.code != sock::SockErr::None) {
            LOG("connection to nodeid=", peer_info.id, " failed");
            //print_socket_result(result);
            evtlog::info(elog_kind::Connect_Failed, elog_cat::SocketMonitor, peer_info.id);
            return false;
        }

        // send them a notice of who we are
        if (!send_id(client.get())) {
            ERR_PRINT(__func__, "(): send ID failed unexpectedly during connection attempt");
            evtlog::warn(elog_kind::Connect_Send_Failed, elog_cat::SocketMonitor, peer_info.id);
            return false;
        }

        // set the peer id for this socket, and replace socket 
        // in registry with this socket
        // NOTE: when replacing a socket, we assume that someone before us has 
        // handled closing the socket and killing the socket recv worker for that socket
        client->set_destination_id(peer_info.id);
        m_router.upsert_socket(
            peer_info.id, 
            std::move(client)
        );
        
        // start up thread to listen to this socket
        start_remote_recv_worker(peer_info.id);

        LOG("established tcp connection to nodeid=", peer_info.id);
        evtlog::info(elog_kind::Connect_Success, elog_cat::SocketMonitor, peer_info.id);
        return true;
    }

    void ConnectionManager::ping_remote_peer(addr::NodeAddress peer_info, std::shared_ptr<sock::TCPClient> client) {
        if (client->get_destination_id() != peer_info.id) {
            ERR_PRINT(__func__, "(): got mismatching IDs for socket and info, socket list corrupted");
            return;
        }

        // if connected, everything is fine
        if (client->is_connected()) {
            bool connected = send_ping(client.get());
            if (connected) return;
        }

        // do socket disconnect logic, this wont do anything if already disconnected
        client->disconnect();
        LOG("found dead socket to nodeid=", peer_info.id);
        evtlog::info(elog_kind::DeadSocket_Found, elog_cat::SocketMonitor, peer_info.id);

        // if worker is still listening to dead socket, stop and erase them
        stop_remote_recv_worker(peer_info.id);

        // do connection logic
        connect_to_remote_peer(peer_info);
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