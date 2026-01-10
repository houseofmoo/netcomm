#include <eROIL/eroil.h>
#include <memory>
#include <vector>
#include "config/config.h"
#include "manager/manager.h"


static std::unique_ptr<eroil::Manager> manager;

void init_manager(int id) {
    auto node_info = eroil::GetTestNodeInfo();
    manager = std::make_unique<eroil::Manager>(id, node_info);
    manager->init();
}