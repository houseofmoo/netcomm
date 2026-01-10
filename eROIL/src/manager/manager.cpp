#include "manager.h"
#include <cstring>
#include <thread>
#include <chrono>
#include <memory>
#include <eROIL/print.h>
#include "socket/socket_result.h"
#include "socket/socket_header.h"

namespace eroil {
    static int unique_id() {
        static std::atomic<int> next_id{0};
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    Manager::Manager(int id, std::vector<NodeInfo> nodes) 
        : m_id(id), 
        m_address_book{}, 
        m_router{}, 
        m_broadcast{id, m_address_book, m_router}, 
        m_comms{id, m_router, m_address_book},
        m_sender(m_router),
        m_valid(false) {

        // confirm we know who the fuck we are
        if (static_cast<size_t>(m_id) > nodes.size()) {
            ERR_PRINT("manager has no entry in nodes info list, manager cannot initialize");
            m_valid = false;
            return;
        }
        m_valid = true;
        m_address_book.insert_addresses(nodes[m_id], nodes);
    }

    bool Manager::init() {
        if (!m_valid) {
            ERR_PRINT("manager cannot initialize! figure out wtf is wrong");
            return false;
        }

        m_sender.start();                   // start sender thread
        m_broadcast.start_broadcast(30001); // join udp multicast group and send/recv threads
        m_comms.start();                    // start tcp listener and recv comms handlers
        return true;
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

        m_sender.enqueue(worker::SendQEntry{
            handle->uid,
            handle->data.label,
            send_size,
            std::move(data)
        });
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
}