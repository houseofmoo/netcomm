#include "config.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <eROIL/print.h>

namespace eroil {
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
            print::error("node IDs in node info were invalid (id < 0)");
            return {};
        }
        
        const NodeInfo invalid{ -1, {}, 0 };
        std::vector<NodeInfo> indexable(static_cast<size_t>(largest_id) + 1, invalid);

        // build indexable list
        for  (auto& node : nodes) {
            const auto index = static_cast<size_t>(node.id);
            if (indexable[index].id >= 0) {
                // duplicate found, this is a major config file problem
                print::error("FOUND A DUPLICATE NODE ID IN NODEINFO LIST, exiting");
                return {};
            }

            indexable[index] = std::move(node);
        }

        return indexable;
    }

    std::vector<std::vector<std::string>> ParseCSV(const std::string& filename) {
        std::vector<std::vector<std::string>> rows;

        std::ifstream file(filename);
        if (!file.is_open()) {
            print::error("could not open file ", filename);
            return rows;
        }

        print::write("reading ", filename);
        std::string line;
        while (std::getline(file, line)) {
            std::vector<std::string> columns;
            std::stringstream ss(line);
            std::string cell;

            // line starts with # ignore it
            if (line.rfind("#", 0) == 0) continue;

            while (std::getline(ss, cell, ',')) {
                columns.push_back(cell);
            }
            rows.push_back(columns);
        }

        return rows;
    }

    std::vector<NodeInfo> GetNodeInfo(const std::string_view& filename) {
        auto rows = ParseCSV(std::string(filename));
        if (rows.size() <= 0) {
            print::error("no node information, file did not exist or was empty/unparsable");
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
            print::error("no node information, could not build node info list from parsed data");
            return {};
        }

        return make_indexable_by_id(nodes);
    }

    std::vector<NodeInfo> GetNodeInfo() {
        std::vector<NodeInfo> nodes;
        print::write("building FAKE NodeInfo for testing (all local addrs)");
        for (int i = 0; i < 20; i++) {
            nodes.push_back(NodeInfo{
                i,
                LOCAL_HOST,
                static_cast<uint16_t>(PORT_START + i)
            });
        }
        
        return make_indexable_by_id(nodes);
    }

    ManagerConfig get_manager_cfg(int id, ManagerMode mode) {
        std::vector<NodeInfo> nodes;

         switch (mode) {
            case ManagerMode::TestMode_Local_ShmOnly: // fallthrough
            case ManagerMode::TestMode_Lopcal_SocketOnly: // fallthrough
            case ManagerMode::TestMode_Sim_Network: nodes = GetNodeInfo(); break;
            
            case ManagerMode::Normal: // fallthrough
            default: nodes = GetNodeInfo(MANAGE_CONFIG_FILE_PATH); break;
        }
        
        return ManagerConfig{
            static_cast<NodeId>(id),
            mode,
            nodes
        };
    }


}  // namespace eROIL
