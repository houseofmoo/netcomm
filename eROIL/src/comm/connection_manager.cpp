#include "connection_manager.h"
#include <thread>
#include <chrono>
#include <eROIL/print.h>
#include "types/label_io_types.h"
#include "log/evtlog_api.h"

namespace eroil::comm {
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
        m_local_sender{},
        m_remote_sender{},
        m_shm_recvr{router, id},
        m_sock_recvrs{} {}

    bool ConnectionManager::start() {
        // start sender thread workers
        m_local_sender.start();
        m_remote_sender.start();

        // open shm recv block
        if (!m_router.open_recv_shm(m_id)) {
            ERR_PRINT(" CRITICAL! unable to open recv shm block, manager ini failure");
            return false;
        }
        LOG("shm recv block created, starting shm recv worker");
        m_shm_recvr.start();

        // tcp server listener thread
        std::thread([this]() { run_tcp_server(); }).detach();

        // split peers into local and remote
        addr::PeerSet peers = addr::get_peer_set(m_id);

        // connect to peers with a id < ours
        initial_remote_connection(peers.remote_connect_to);

        // start monitor thread
        std::thread([this]() { remote_connection_monitor(); }).detach();

        // open local peers shared memory blocks
        spawn_local_shm_opener(peers.local);
        
        return true;
    }

    void ConnectionManager::initial_remote_connection(std::vector<addr::NodeAddress> remote_peers) {
        // attempt to connect to remote peers a few times
        // if this fails and exit, monitor thread will continue trying
        const int max_attempts = 5;
        const int expected = remote_peers.size();
        for (int attempts = 0; attempts < max_attempts; ++attempts) {
            int connected = 0;
            for (const addr::NodeAddress& info : remote_peers) {
                if (m_router.get_socket(info.id) != nullptr) {
                    connected += 1;
                    continue;
                }
                
                if (connect_to_remote_peer(info)) {
                    connected += 1;
                }
            }

            // if we found all peers we expected to connect to, we're done
            if (connected >= expected) {
                LOG("connected to ", connected, " out of ", expected, " expected remote peers");
                break;
            }

            if (attempts + 1 < max_attempts) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
            }
        }
    }

    void ConnectionManager::spawn_local_shm_opener(std::vector<addr::NodeAddress> local_peers) {
        // this continues trying every 5 seconds until it finds all expected local shared memory blocks
        // incase someone joins the party late
        std::thread([this, local_peers]() {
            const int expected = local_peers.size();
            int found = 0;

            while (true) {
                for (const addr::NodeAddress& info : local_peers) {
                    if (m_router.get_send_shm(info.id) != nullptr) continue;

                    if (m_router.open_send_shm(info.id)) {
                        LOG("established shm send block to nodeid=", info.id);
                        found += 1;
                    } else {
                        LOG("shm send block to nodeid=", info.id, " does not yet exist, cannot open");
                    }
                }

                if (found >= expected) {
                    LOG("opened shm send blocks for ", found, " out of ", expected , " expected local peers");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
            }
        }).detach();
    }

    void ConnectionManager::enqueue_send(handle_uid uid, Label label, io::SendBuf send_buf) {
        auto [err, job] = m_router.build_send_job(m_id, label, uid, std::move(send_buf));
        if (err != io::SendJobErr::None) {
            return; 
        }

        if (job->local_recvrs.empty() && job->remote_recvrs.empty()) {
            job->finalize_iosb();
            return;
        }

        if (!job->local_recvrs.empty()) {
            m_local_sender.enqueue(job);
        }

        if (!job->remote_recvrs.empty()) {
            m_remote_sender.enqueue(job);
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
            std::make_unique<wrk::SocketRecvWorker>(m_router, m_id, peer_id)
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
        addr::NodeAddress info = addr::get_address(m_id);
        LOG("tcp server listen start at ", info.ip, ":", info.port);
        EvtMark mark(elog_cat::TCPServer);

        sock::SockResult ts_result = m_tcp_server.open_and_listen(info.port);
        if (ts_result.code != sock::SockErr::None) {
            ERR_PRINT("tcp server open failed, exiting");
            evtlog::error(elog_kind::StartFailed, elog_cat::TCPServer);
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
            io::LabelHeader hdr{};
            sock::SockResult recv_result = client->recv_all(&hdr, sizeof(hdr));
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

            if (!io::has_flag(hdr.flags, io::LabelFlag::Connect)) {
                ERR_PRINT("tcp server recvd invalid request type");
                evtlog::warn(elog_kind::InvalidFlags, elog_cat::TCPServer, hdr.source_id);
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
        addr::PeerSet peers = addr::get_peer_set(m_id);
        if (peers.remote.empty()) {
            LOG("no remote peers to monitor, remote peer monitor thread exits");
            return;
        }

        while (true) {
            EvtMark mark(elog_cat::SocketMonitor);
            for (const addr::NodeAddress& info : peers.remote) {
                std::shared_ptr<sock::TCPClient> socket = m_router.get_socket(info.id);
                if (socket == nullptr) {
                    // if connection does not exist, connect
                    evtlog::info(elog_kind::Connect, elog_cat::SocketMonitor);
                    connect_to_remote_peer(info);
                } else {
                    // otherwise check socket connected
                    evtlog::info(elog_kind::Ping, elog_cat::SocketMonitor);
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

        // attempt connection
        LOG("attempt connection to nodeid=", peer_info.id, " ip=", peer_info.ip, ":", peer_info.port);
        auto client = std::make_shared<sock::TCPClient>();
        sock::SockResult result = client->open_and_connect(peer_info.ip.c_str(), peer_info.port);
        if (result.code != sock::SockErr::None) {
            LOG("connection to nodeid=", peer_info.id, " failed");
            //print_socket_result(result);
            evtlog::info(elog_kind::ConnectionFailed, elog_cat::SocketMonitor, peer_info.id);
            return false;
        }

        // send them a notice of who we are
        if (!send_id(client.get())) {
            ERR_PRINT("send ID failed unexpectedly during connection attempt");
            evtlog::warn(elog_kind::SendFailed, elog_cat::SocketMonitor, peer_info.id);
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
        evtlog::info(elog_kind::NewConnection, elog_cat::SocketMonitor, peer_info.id);
        return true;
    }

    void ConnectionManager::ping_remote_peer(addr::NodeAddress peer_info, std::shared_ptr<sock::TCPClient> client) {
        if (client->get_destination_id() != peer_info.id) {
            ERR_PRINT("got mismatching IDs for socket and info, socket list corrupted");
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
        evtlog::info(elog_kind::DeadSocketFound, elog_cat::SocketMonitor, peer_info.id);

        // if worker is still listening to dead socket, stop and erase them
        stop_remote_recv_worker(peer_info.id);

        // do connection logic
        connect_to_remote_peer(peer_info);
    }

    bool ConnectionManager::send_id(sock::TCPClient* sock) {
        // send them a notice of who we are
        io::LabelHeader hdr{};
        hdr.magic = MAGIC_NUM;
        hdr.version = VERSION;
        hdr.source_id = m_id;
        hdr.flags = static_cast<uint16_t>(io::LabelFlag::Connect);
        hdr.label = 0;
        hdr.data_size = 0;

        sock::SockResult err = sock->send_all(&hdr, sizeof(hdr));
        return map_sock_failures(err.code);
    }

    bool ConnectionManager::send_ping(sock::TCPClient* sock) {
        // send ping header
        io::LabelHeader hdr{};
        hdr.magic = MAGIC_NUM;
        hdr.version = VERSION,
        hdr.source_id = m_id,
        hdr.flags = static_cast<uint16_t>(io::LabelFlag::Ping),
        hdr.label = 0,
        hdr.data_size = 0;

        sock::SockResult err = sock->send_all(&hdr, sizeof(hdr));
        return map_sock_failures(err.code);
    }
}