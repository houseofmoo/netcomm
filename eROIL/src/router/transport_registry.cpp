#include "transport_registry.h"

#include <utility>   // std::move
#include <eROIL/print.h>

namespace eroil {
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

    std::vector<std::shared_ptr<sock::TCPClient>> TransportRegistry::get_all_sockets() const {
        std::vector<std::shared_ptr<sock::TCPClient>> out;
        for (auto [id, sock] : m_sockets) {
            out.push_back(sock);
        }
        return out;
    }

    // send shm
    bool TransportRegistry::open_send_shm(Label label, size_t label_size) {
        if (has_send_shm(label)) return true;

        auto shm_block = std::make_shared<shm::Shm>(label, label_size);
        auto open_err = shm_block->create_or_open();
        if (open_err != shm::ShmErr::None) {
            ERR_PRINT("open_send_shm(): create_or_open failed label=", label);
            return false;
        }

        m_send_shm.emplace(label, std::move(shm_block));
        return true;
    }

    bool TransportRegistry::delete_send_shm(Label label) {
        auto it = m_send_shm.find(label);
        if (it == m_send_shm.end()) return false;

        it->second->close();
        m_send_shm.erase(it);
        return true;
    }

    std::shared_ptr<shm::Shm> TransportRegistry::get_send_shm(Label label) const noexcept {
        auto it = m_send_shm.find(label);
        if (it == m_send_shm.end()) return nullptr;
        return it->second;
    }

    bool TransportRegistry::has_send_shm(Label label) const noexcept {
        auto it = m_send_shm.find(label);
        return (it != m_send_shm.end()) && (it->second != nullptr);
    }

    // recv shm
    bool TransportRegistry::open_recv_shm(Label label, size_t label_size) {
        if (has_send_shm(label)) return true;

        auto shm_block = std::make_shared<shm::Shm>(label, label_size);
        auto open_err = shm_block->create_or_open();
        if (open_err != shm::ShmErr::None) {
            ERR_PRINT("open_recv_shm(): create_or_open failed label=", label);
            return false;
        }

        m_recv_shm.emplace(label, std::move(shm_block));
        return true;
    }

    bool TransportRegistry::delete_recv_shm(Label label) {
        auto it = m_recv_shm.find(label);
        if (it == m_recv_shm.end()) return false;

        it->second->close();
        m_recv_shm.erase(it);
        return true;
    }

    std::shared_ptr<shm::Shm> TransportRegistry::get_recv_shm(Label label) const noexcept {
        auto it = m_recv_shm.find(label);
        if (it == m_recv_shm.end()) return nullptr;
        return it->second;
    }

    bool TransportRegistry::has_recv_shm(Label label) const noexcept {
        auto it = m_recv_shm.find(label);
        return (it != m_recv_shm.end()) && (it->second != nullptr);
    }
}