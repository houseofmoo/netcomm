#include "config.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <filesystem>
#include <unordered_map>
#include <eROIL/print.h>

namespace eroil::cfg {
    std::unordered_map<std::string, std::string> parse_kv_file(const std::string& path) {
        std::unordered_map<std::string, std::string> out;

        std::ifstream file(path);
        if (!file.is_open()) {
            ERR_PRINT("could not open file ", path);
            return out;
        }

        LOG("reading ", path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line.rfind("#", 0) == 0) continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            out[line.substr(0, pos)] = line.substr(pos + 1);
        }
        return out;
    }

    ManagerConfig get_manager_cfg(int id) {
        ManagerConfig cfg;
        cfg.id = id;
        cfg.mode = ManagerMode::Normal;

        auto kv = parse_kv_file(std::string(MANAGE_CONFIG_FILE_PATH));

        // get mode
        if (kv.count("mode")) {
            std::string mode_str = kv["mode"];
            if (mode_str == "TestMode_Local_ShmOnly") {
                cfg.mode = ManagerMode::TestMode_Local_ShmOnly;
            } else if (mode_str == "TestMode_Local_SocketOnly") {
                cfg.mode = ManagerMode::TestMode_Local_SocketOnly;
            } else if (mode_str == "TestMode_Sim_Network") {
                cfg.mode = ManagerMode::TestMode_Sim_Network;
            } 
        }

        // get udp config
        cfg.mcast_cfg = UdpMcastConfig{};
        if (kv.count("mcast_group_ip")) {
            cfg.mcast_cfg.group_ip = kv["mcast_group_ip"].c_str();
        }
        if (kv.count("mcast_port")) {
            cfg.mcast_cfg.port = static_cast<uint16_t>(std::stoi(kv["mcast_port"]));
        }
        if (kv.count("mcast_bind_ip")) {
            cfg.mcast_cfg.bind_ip = kv["mcast_bind_ip"].c_str();
        }
        if (kv.count("mcast_ttl")) {
            cfg.mcast_cfg.ttl = std::stoi(kv["mcast_ttl"]);
        }
        if (kv.count("mcast_loopback")) {
            cfg.mcast_cfg.loopback = kv["mcast_loopback"] == "true";
        }
        if (kv.count("mcast_reuse_addr")) {
            cfg.mcast_cfg.reuse_addr = kv["mcast_reuse_addr"] == "true";
        }

        return cfg;
    }
}
