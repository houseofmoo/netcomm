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

        print::write("reading ", path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line.rfind("#", 0) == 0) continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            out[line.substr(0, pos)] = line.substr(pos + 1);
        }
        return out;
    }

    std::vector<std::vector<std::string>> parse_csv_file(const std::string& path) {
        std::vector<std::vector<std::string>> rows;

        std::ifstream file(path);
        if (!file.is_open()) {
            ERR_PRINT("could not open file ", path);
            return rows;
        }

        print::write("reading ", path);
        std::string line;
        while (std::getline(file, line)) {
            std::vector<std::string> columns;
            std::stringstream ss(line);
            std::string cell;

            if (line.empty() || line.rfind("#", 0) == 0) continue;

            while (std::getline(ss, cell, ',')) {
                columns.push_back(cell);
            }
            rows.push_back(columns);
        }

        return rows;
    }

    std::vector<NodeInfo> make_indexable_by_id(std::vector<NodeInfo>& nodes) {
        if (nodes.empty())  {
            return {};
        }

        // validate IDs are > 0
        NodeId largest_id  = std::numeric_limits<NodeId>::min();
        NodeId smallest_id = std::numeric_limits<NodeId>::max();
        for (const auto& node : nodes) {
            largest_id  = (node.id > largest_id)  ? node.id : largest_id;
            smallest_id = (node.id < smallest_id) ? node.id : smallest_id;
        }

        // enforce IDs must be >= 0
        if (smallest_id < 0) {
            ERR_PRINT("node IDs in node info were invalid (id < 0)");
            return {};
        }
        
        const NodeInfo invalid{ -1, {}, 0 };
        std::vector<NodeInfo> indexable(static_cast<size_t>(largest_id) + 1, invalid);

        // build indexable list
        for  (auto& node : nodes) {
            const auto index = static_cast<size_t>(node.id);
            if (indexable[index].id >= 0) {
                // duplicate found, this is a major config file problem
                ERR_PRINT("FOUND A DUPLICATE NODE ID IN NODEINFO LIST, exiting");
                return {};
            }

            indexable[index] = std::move(node);
        }

        return indexable;
    }

    std::vector<NodeInfo> build_node_info(const std::string_view& filename) {
        auto rows = parse_csv_file(std::string(filename));
        if (rows.size() <= 0) {
            ERR_PRINT("no node information, file did not exist or was empty/unparsable");
            return {};
        }
        
        print::write("building NodeInfo from ", filename);
        std::vector<NodeInfo> nodes;
        for (auto row : rows) {
            nodes.push_back(NodeInfo{
                static_cast<int>(std::stoi(row[0])),
                row[1],
                static_cast<uint16_t>(std::stoi(row[2]))
            });
        }

        if (nodes.empty()) {
            ERR_PRINT("no node information, could not build node info list from parsed data");
            return {};
        }

        return make_indexable_by_id(nodes);
    }

    std::vector<NodeInfo> build_fake_node_info() {
        std::vector<NodeInfo> nodes;
        PRINT("building FAKE NodeInfo for testing");
        for (int i = 0; i < 20; i++) {
            nodes.push_back(NodeInfo{
                i,
                detail::LOCAL_HOST,
                static_cast<uint16_t>(detail::PORT_START + i)
            });
        }
        
        return make_indexable_by_id(nodes);
    }

    ManagerConfig get_manager_cfg(int id) {
        ManagerConfig cfg;
        cfg.id = id;
        cfg.mode = ManagerMode::Normal;

        auto kv = parse_kv_file(std::string(detail::MANAGE_CONFIG_FILE_PATH));

        // get mode
        if (kv.count("mode")) {
            std::string mode_str = kv["mode"];
            if (mode_str == "TestMode_Local_ShmOnly") {
                cfg.mode = ManagerMode::TestMode_Local_ShmOnly;
            } else if (mode_str == "TestMode_Lopcal_SocketOnly") {
                cfg.mode = ManagerMode::TestMode_Lopcal_SocketOnly;
            } else if (mode_str == "TestMode_Sim_Network") {
                cfg.mode = ManagerMode::TestMode_Sim_Network;
            } 
        }

        // get peer info
        switch (cfg.mode) {
            case ManagerMode::TestMode_Local_ShmOnly: // fallthrough
            case ManagerMode::TestMode_Lopcal_SocketOnly: // fallthrough
            case ManagerMode::TestMode_Sim_Network: cfg.nodes = build_fake_node_info(); break;
            
            case ManagerMode::Normal: // fallthrough
            default: cfg.nodes = build_node_info(detail::PEER_IP_FILE_PATH); break;
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
}  // namespace eROIL
