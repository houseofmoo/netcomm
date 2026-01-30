#include "transport_registry.h"

#include <utility>   // std::move
#include <eROIL/print.h>

namespace eroil::rt {
    // socket
    bool TransportRegistry::upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock) {
        if (sock == nullptr) return false;

        auto it = m_sockets.find(id);
        if (it != m_sockets.end()) {
            it->second->disconnect();
        }

        m_sockets.insert_or_assign(id, std::move(sock));
        return true;
    }

    bool TransportRegistry::delete_socket(NodeId id) {
        auto it = m_sockets.find(id);
        if (it == m_sockets.end()) return false;

        it->second->disconnect();
        m_sockets.erase(it);
        return true;
    }

    std::shared_ptr<sock::TCPClient> TransportRegistry::get_socket(NodeId id) const noexcept {
        auto it = m_sockets.find(id);
        if (it == m_sockets.end()) return nullptr;
        return it->second;
    }

    bool TransportRegistry::has_socket(NodeId id) const noexcept {
        auto it = m_sockets.find(id);
        return (it != m_sockets.end()) && (it->second != nullptr);
    }

    // send shm
    bool TransportRegistry::open_send_shm(NodeId dst_id) {
        if (has_send_shm(dst_id)) return true;

        auto shm = std::make_shared<shm::ShmSend>(dst_id);
        if (!shm->open()) {
            shm->close();
            return false;
        }

        m_send_shm.emplace(dst_id, std::move(shm));
        return true;
    }

    std::shared_ptr<shm::ShmSend> TransportRegistry::get_send_shm(NodeId dst_id) const noexcept {
        auto it = m_send_shm.find(dst_id);
        if (it == m_send_shm.end()) return nullptr;
        return it->second;
    }

    bool TransportRegistry::has_send_shm(NodeId dst_id) const noexcept {
        auto it = m_send_shm.find(dst_id);
        return (it != m_send_shm.end()) && (it->second != nullptr);
    }

    // recv shm
    bool TransportRegistry::open_recv_shm(NodeId my_id) {
        if (m_recv_shm != nullptr) return true;

        m_recv_shm = std::make_shared<shm::ShmRecv>(my_id);
        if (!m_recv_shm->create_or_open()) {
            ERR_PRINT("create_or_open failed for recv shm");
            m_recv_shm->close();
            m_recv_shm.reset();
            return false;
        }

        return true;
    }

    std::shared_ptr<shm::ShmRecv> TransportRegistry::get_recv_shm() const noexcept {
        return m_recv_shm;
    }
}