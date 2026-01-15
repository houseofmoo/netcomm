#include <memory>
#include <vector>
#include <string_view>
#include <eROIL/eroil_c.h>
#include <eROIL/print.h>
#include "config/config.h"
#include "manager/manager.h"
#include "types/handles.h"
#include "types/types.h"


static std::unique_ptr<eroil::Manager> manager;
static eroil::ManagerConfig config{};

static bool initialized = false;

int NAE_Init(int /*iModuleId*/, int /*iProgramIDOffset*/, int /*iManager_CPU_ID*/, int /*iMaxNumCpus*/, int iNodeId) {
    config = eroil::get_manager_cfg(iNodeId, eroil::ManagerMode::Normal);
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
bool init_manager(int id) {
    print::set_id(id);
    config = eroil::get_manager_cfg(id, eroil::ManagerMode::TestMode_Sim_Network);
    manager = std::make_unique<eroil::Manager>(config);
    initialized = manager->init();
    return initialized;
}

void* open_send(int label, void* buf, int size) {
    if (manager == nullptr || !initialized) {
        ERR_PRINT("manager not initialized");
        return nullptr;
    }
    eroil::OpenSendData data;
    data.label = label;
    data.buf = static_cast<uint8_t*>(buf);
    data.buf_size = static_cast<size_t>(size);
    data.buf_offset = 0;
    data.sem = nullptr;
    data.iosb = nullptr;
    data.num_iosb = 0;
    data.iosb_index = 0;

    auto ptr = manager->open_send(data);
    return reinterpret_cast<void*>(ptr);
}

void* open_recv(int label, void* buf, int size, void* sem) {
    if (manager == nullptr || !initialized) {
        ERR_PRINT("manager not initialized");
        return nullptr;
    }

    eroil::OpenReceiveData data;
    data.label = label;
    data.forward_label = 0;
    data.buf = static_cast<uint8_t*>(buf);
    data.buf_size = static_cast<size_t>(size);
    data.buf_slots = 1;
    data.buf_index = 0;
    data.aux_buf = nullptr;
    data.sem = static_cast<eroil::sem_handle>(sem);
    data.iosb = nullptr;
    data.num_iosb = 0;
    data.iosb_index = 0;
    data.signal_mode = 0;
    auto ptr = manager->open_recv(data);

    return reinterpret_cast<void*>(ptr);
}

void send_label(void* handle, void* buf, int buf_size, int buf_offset) {
    if (manager == nullptr || !initialized) {
        ERR_PRINT("manager not initialized");
        return;
    }

    manager->send_label(
        reinterpret_cast<eroil::SendHandle*>(handle), 
        buf, 
        static_cast<size_t>(buf_size),
        static_cast<size_t>(buf_offset)
    );
}