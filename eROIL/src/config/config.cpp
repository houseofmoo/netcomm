#include "config.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <eROIL/print.h>

namespace eroil {
    std::vector<std::vector<std::string>> ParseCSV(const std::string& filename) {
        std::vector<std::vector<std::string>> rows;

        std::ifstream file(filename);
        if (!file.is_open()) {
            print::error("could not open file ", filename);
            return rows;
        }

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

    std::vector<NodeInfo> GetNodeInfo() {
        std::vector<NodeInfo> nodes;
        auto rows = ParseCSV("etc/manager_ips.txt");

        if (rows.size() > 0) {
            print::write("using manager_ips.txt config");
            for (auto row : rows) {
                nodes.push_back(NodeInfo{
                    static_cast<int>(std::stoi(row[0])),
                    row[1],
                    static_cast<uint16_t>(std::stoi(row[2]))
                });
            }
        } else {
            print::write("using test config");
            for (int i = 0; i < 5; i++) {
                nodes.push_back(NodeInfo{
                    i,
                    LOCAL_HOST,
                    static_cast<uint16_t>(PORT_START + i)
                });
            }
        }

        return nodes;
    }

    std::vector<NodeInfo> GetTestNodeInfo() {
        std::vector<NodeInfo> nodes;
        for (int i = 0; i < 20; i++) {
            nodes.push_back(NodeInfo{
                i,
                LOCAL_HOST,
                static_cast<uint16_t>(PORT_START + i)
            });
        }
        return nodes;
    }
}  // namespace eROIL
