#include <eROIL/eroil_c.h>
#include <memory>
#include <vector>
#include "config/config.h"
#include "manager/manager.h"
#include "types/handles.h"


static std::unique_ptr<eroil::Manager> manager;
static eroil::ManagerConfig config{};
static bool initialized = false;

int NAE_Init(int /*iModuleId*/, int /*iProgramIDOffset*/, int /*iManager_CPU_ID*/, int /*iMaxNumCpus*/, int iNodeId) {
    config.id = iNodeId;
    config.nodes = eroil::GetTestNodeInfo();
    config.mode = eroil::ManagerMode::LocalOnlyTestMode;
    manager = std::make_unique<eroil::Manager>(config);
    initialized = manager->init();
    return initialized ? 1 : 0;
}

int NAE_Is_Agent_Up(int /*iNodeId*/) {
    return initialized ? 1 : 0;
}

int NAE_Get_Node_ID() {
    return config.id;
}

int NAE_Get_ROIL_Node_ID() {
    return config.id;
}

// old interface test
void init_manager(int id) {
    auto cfg = eroil::ManagerConfig();
    cfg.id = id;
    cfg.nodes = eroil::GetTestNodeInfo();
    cfg.mode = eroil::ManagerMode::LocalOnlyTestMode;
    manager = std::make_unique<eroil::Manager>(cfg);
    manager->init();
    initialized = true;
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