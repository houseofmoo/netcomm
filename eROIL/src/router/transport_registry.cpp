#include "transport_registry.h"

#include <utility>   // std::move
#include <eROIL/print.h>

namespace eroil {
    static bool ok_send_event_err(shm::ShmOpErr err) {
        return (err == shm::ShmOpErr::None) || (err == shm::ShmOpErr::DuplicateSendEvent);
    }

    bool TransportRegistry::has_socket(NodeId id) const {
        auto it = m_sockets.find(id);
        return (it != m_sockets.end()) && (it->second != nullptr);
    }

    bool TransportRegistry::upsert_socket(NodeId id, std::shared_ptr<sock::TCPClient> sock) {
        if (!sock) return false;
        m_sockets.insert_or_assign(id, std::move(sock));
        return true;
    }

    std::shared_ptr<sock::TCPClient> TransportRegistry::get_socket(NodeId id) const {
        auto it = m_sockets.find(id);
        if (it == m_sockets.end()) return nullptr;
        return it->second;
    }

    std::shared_ptr<shm::ShmSend>
    TransportRegistry::ensure_send_shm(Label label, size_t size, NodeId to_id) {
        if (auto it = m_send_shm.find(label); it != m_send_shm.end() && it->second) {
            auto evt_err = it->second->add_send_event(to_id);
            if (!ok_send_event_err(evt_err)) {
                ERR_PRINT("ensure_send_shm(): failed add_send_event label=", label, " to_id=", to_id);
                return nullptr;
            }
            return it->second;
        }

        auto shm_block = std::make_shared<shm::ShmSend>(label, size);
        auto open_err = shm_block->create_or_open();
        if (open_err != shm::ShmErr::None) {
            ERR_PRINT("ensure_send_shm(): create_or_open failed label=", label);
            return nullptr;
        }

        auto evt_err = shm_block->add_send_event(to_id);
        if (evt_err != shm::ShmOpErr::None) {
            ERR_PRINT("ensure_send_shm(): add_send_event failed label=", label, " to_id=", to_id);
            return nullptr;
        }

        m_send_shm.emplace(label, shm_block);
        return shm_block;
    }

    bool TransportRegistry::upsert_send_shm(Label label, std::shared_ptr<shm::ShmSend> shm) {
        if (!shm) return false;

        m_send_shm.insert_or_assign(label, std::move(shm));
        return true;
    }

    std::shared_ptr<shm::ShmSend> TransportRegistry::get_send_shm(Label label) {
        auto it = m_send_shm.find(label);
        if (it == m_send_shm.end()) return nullptr;
        return it->second;
    }

    std::shared_ptr<shm::ShmRecv>
    TransportRegistry::ensure_recv_shm(Label label, size_t size, NodeId my_id) {
        if (auto it = m_recv_shm.find(label); it != m_recv_shm.end() && it->second) {
            auto evt_err = it->second->set_recv_event(my_id);
            if (evt_err != shm::ShmOpErr::None) {
                ERR_PRINT("ensure_recv_shm(): failed set_recv_event label=", label, " my_id=", my_id);
                return nullptr;
            }
            return it->second;
        }

        auto shm_block = std::make_shared<shm::ShmRecv>(label, size);
        auto open_err = shm_block->create_or_open();
        if (open_err != shm::ShmErr::None) {
            ERR_PRINT("ensure_recv_shm(): create_or_open failed label=", label);
            return nullptr;
        }

        auto evt_err = shm_block->set_recv_event(my_id);
        if (evt_err != shm::ShmOpErr::None) {
            ERR_PRINT("ensure_recv_shm(): set_recv_event failed label=", label, " my_id=", my_id);
            return nullptr;
        }

        m_recv_shm.emplace(label, shm_block);
        return shm_block;
    }

    bool TransportRegistry::upsert_recv_shm(Label label, std::shared_ptr<shm::ShmRecv> shm) {
        if (!shm) return false;

        m_recv_shm.insert_or_assign(label, std::move(shm));
        return true;
    }

    std::shared_ptr<shm::ShmRecv> TransportRegistry::get_recv_shm(Label label) {
        auto it = m_recv_shm.find(label);
        if (it == m_recv_shm.end()) return nullptr;
        return it->second;
    }

    bool TransportRegistry::has_local_send_subscriber(Label label, NodeId to_id) const {
        auto it = m_send_shm.find(label);
        if (it == m_send_shm.end() || !it->second) return false;
        return it->second->has_send_event(label, to_id);
    }

    bool TransportRegistry::has_local_recv_publisher(Label label, NodeId my_id) const {
        auto it = m_recv_shm.find(label);
        if (it == m_recv_shm.end() || !it->second) return false;
        return it->second->has_recv_event(label, my_id);
    }

    SendTargets TransportRegistry::resolve_send_targets(Label label,
                                                        bool want_local,
                                                        const std::vector<NodeId>& remote_node_ids) const {
        SendTargets out{};

        // local shm
        if (want_local) {
            if (auto it = m_send_shm.find(label); it != m_send_shm.end() && it->second) {
                out.shm = it->second;
                out.has_local = true;
            }
        }

        // remote sockets
        if (!remote_node_ids.empty()) {
            out.sockets.reserve(remote_node_ids.size());
            for (NodeId peer : remote_node_ids) {
                auto sit = m_sockets.find(peer);
                if (sit != m_sockets.end() && sit->second) {
                    out.sockets.push_back(sit->second);
                }
            }
            out.has_remote = !out.sockets.empty();
        }

        return out;
    }

    void TransportRegistry::erase_send_shm(Label label) {
        m_send_shm.erase(label);
    }

    void TransportRegistry::erase_recv_shm(Label label) {
        m_recv_shm.erase(label);
    }
}
