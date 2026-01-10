#include <eROIL/eroil.h>
#include <memory>
#include <vector>
#include "config/config.h"
#include "manager/manager.h"
#include "types/handles.h"


static std::unique_ptr<eroil::Manager> manager;

void init_manager(int id) {
    auto node_info = eroil::GetTestNodeInfo();
    manager = std::make_unique<eroil::Manager>(id, node_info);
    manager->init();
}

void* open_send(int label, void* buf, int size) {
    auto ptr = manager->open_send({
        label,
        static_cast<uint8_t*>(buf),
        static_cast<size_t>(size),
        0,
        nullptr,
        nullptr,
        0,
        0
    });
    
    return reinterpret_cast<void*>(ptr);
}

void* open_recv(int label, void* buf, int size, void* sem) {
    auto ptr = manager->open_recv({
        label,
        0,
        static_cast<uint8_t*>(buf),
        static_cast<size_t>(size),
        1,
        0,
        nullptr,
        sem,
        nullptr,
        0,
        0,
        0
    });

    return reinterpret_cast<void*>(ptr);
}

void send_label(void* handle, void* buf, int buf_size, int buf_offset) {
    manager->send_label(
        reinterpret_cast<eroil::SendHandle*>(handle), 
        buf, 
        static_cast<size_t>(buf_size),
        static_cast<size_t>(buf_offset)
    );
}