#include "recvrs.h"
#include <thread>
#include <chrono>
#include "socket/socket_header.h"
#include <eROIL/print.h>

namespace eroil {
    Comms::Comms(NodeId id, Router& router, Address& address) : 
        m_id(id),
        m_address_book(address),
        m_router(router), 
        m_context{}, 
        m_tcp_server{}, 
        m_sock_recvrs{}, 
        m_shm_recvrs{} {}

    void Comms::start() {
        start_tcp_server();
        search_peers();
    }

    void Comms::start_tcp_server() {
        LOG(m_id, " tcp server listen start...");
         std::thread tcp_server_thread([this]() {
            NodeAddress info = m_address_book.get(m_id);

            auto ts_result = m_tcp_server.open_and_listen(info.port);
            if (ts_result.code != sock::SockErr::None) {
                ERR_PRINT(m_id, " tcp server open failed, exiting");
                print_socket_result(ts_result);
                return;
            }

            while (true) {
                auto [client, result] = m_tcp_server.accept();
                if (result.code != sock::SockErr::None) {
                    ERR_PRINT(m_id, " tcp server accepted connection but there was an error");
                    print_socket_result(result);
                    continue;
                }

                // they connected to us, they'll send a follow up message
                sock::SocketHeader hdr{};
                auto recv_result = client.recv_all(&hdr, sizeof(hdr));
                if (recv_result.code != sock::SockErr::None) {
                    ERR_PRINT(m_id, " tcp server had an error recving client information");
                    print_socket_result(recv_result);
                    client.disconnect();
                    continue;
                }

                if (hdr.magic != sock::kMagic || hdr.version != sock::kWireVer) {
                    ERR_PRINT(m_id, " tcp server recvd invalid header from client");
                    client.disconnect();
                    continue;
                }

                if (!sock::has_flag(hdr.flags, sock::SocketFlag::Connect)) {
                    ERR_PRINT(m_id, " tcp server recvd invalid request type");
                    client.disconnect();
                    continue;
                }

                auto for_router = std::make_shared<sock::TCPClient>(std::move(client));
                auto for_worker = for_router;

                m_router.upsert_socket(hdr.source_id, for_router);

                // TODO: right now we assume all connetions are new, but
                // if someone disconnects we have to figure out how we want to handle that
                auto [it, inserted] = m_sock_recvrs.try_emplace(
                    hdr.source_id,
                    m_router, hdr.source_id, for_worker // pass constructor args to build worker
                );
                
                if (inserted) {
                    it->second.launch();
                    LOG(m_id, " tcp server established connection to node: ", hdr.source_id);
                } else {
                    ERR_PRINT(m_id, " tcp server tried to insert new socket recvr worker but it already existed");
                }
            }
        });
        tcp_server_thread.detach();
    }

    void Comms::search_peers() {
        // this thread should search for all peers with a Id < ours
        // after complete, enter periodic mode where we confirm connections still exist?

        LOG(m_id, " seraching for peers...");
        for (const auto& [id, info] : m_address_book.addresses) {
            //if (info.kind == RouteKind::Shm) continue;
            if (id >= m_id) continue;

            LOG(m_id, " attempt connection to id=", id, " ip=", info.ip, ":", info.port);
            sock::TCPClient client;
            auto result = client.open_and_connect(info.ip.c_str(), info.port);
            if (result.code != sock::SockErr::None) {
                ERR_PRINT(m_id, " connection to ", id, " failed");
                print_socket_result(result);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // send them a notice of who we are
            uint16_t flags = 0;
            sock::set_flag(flags, sock::SocketFlag::Connect);
            sock::SocketHeader hdr {
                sock::kMagic,
                sock::kWireVer,
                m_id,
                id,
                flags,
                0,
                0
            };
            client.send(&hdr, sizeof(hdr));

            auto for_router = std::make_shared<sock::TCPClient>(std::move(client));
            auto for_worker = for_router;

            m_router.upsert_socket(id, for_router);

            // TODO: right now we assume all connetions are new, but
            // if someone disconnects we have to figure out how we want to handle that
            auto [it, inserted] = m_sock_recvrs.try_emplace(
                id,
                m_router, id, for_worker // pass constructor args to build worker
            );
            
            if (inserted) {
                it->second.launch();
                LOG(m_id, " established connection to node: ", id);
            } else {
                ERR_PRINT(m_id, "tried to insert new socket recvr worker but it already existed");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        LOG(m_id, " peer connection complete");
    }
}