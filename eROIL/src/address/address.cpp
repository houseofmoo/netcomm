#include "address.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <eROIL/print.h>

namespace eroil::addr {
    static std::unordered_map<NodeId, NodeAddress> address_book;

    std::vector<std::vector<std::string>> parse_csv_file(const std::string& path) {
        std::vector<std::vector<std::string>> rows;

        std::ifstream file(path);
        if (!file.is_open()) {
            ERR_PRINT("could not open file ", path);
            return rows;
        }

        LOG("reading ", path);
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

    bool init_address_book(NodeId my_id) {
        LOG("initializing address book");

        auto csv_rows = parse_csv_file(std::string(PEER_IP_FILE_PATH));
        if (csv_rows.size() <= 0) {
            ERR_PRINT("no node information, file did not exist or was empty/unparsable");
            return false;
        }

        // build addr book
        for (auto row : csv_rows) {
            const NodeId id = static_cast<NodeId>(std::stoi(row[0]));
            if (id <= INVALID_NODE) {
                ERR_PRINT("foudn invalid node, skipping");
                continue;
            }

            const std::string ip = std::move(row[1]);

            const int16_t port = static_cast<int16_t>(std::stoi(row[2]));
            if (port < 0) {
                ERR_PRINT("found invalid port=", port, " for id=", id, ", skipping");
                continue;
            }

            auto addr = NodeAddress{
                RouteKind::None,
                id,
                std::move(ip),
                static_cast<uint16_t>(port)
            };

            if (address_book.find(id) != address_book.end()) {
                ERR_PRINT("found duplicate nodeid=", id, ", skipping");
                continue;
            }
            address_book.emplace(id, addr);
        }

        // set up route kinds
        auto it = address_book.find(my_id);
        if (it == address_book.end()) {
            ERR_PRINT("my information does not exist in address book, cannot figure out route to peers");
            return false;
        }
        
        auto& self = it->second;
        for (auto& [id, addr] : address_book) {
            if (addr.id == self.id) {
                addr.kind = RouteKind::Self;
            } else if (addr.ip == self.ip) {
                addr.kind = RouteKind::Shm;
            } else {
                addr.kind = RouteKind::Socket;
            }
        }

        return true;
    }

    const std::unordered_map<NodeId, NodeAddress>& get_address_book() {
        return address_book;
    }

    NodeAddress get_address(NodeId id) {
        auto it = address_book.find(id);
        if (it == address_book.end()) {
            return NodeAddress{ RouteKind::None, id, std::string(), 0 };
        }
        return it->second;
    }

    void all_shm_address_book() {
        PRINT("TEST MODE: making shm only address book");
        
        NodeAddress* self = nullptr;
        for (auto& [id, addr] : address_book) {
            if (addr.kind == RouteKind::Self) {
                self = &addr;
                break;
            }
        }
        if (self == nullptr) { // shouldnt happen as long as addr book is initialized
            ERR_PRINT("counldnt find self in address book to make test network");
            return;
        }

        // set up half the nodes to be socket, other half to be shm
        for (auto& [id, addr] : address_book) {
            if (id == self->id) continue;
            addr.kind = RouteKind::Shm;
        }
    }

    void all_socket_address_book() {
        PRINT("TEST MODE: making socket only address book");
        
        NodeAddress* self = nullptr;
        for (auto& [id, addr] : address_book) {
            if (addr.kind == RouteKind::Self) {
                self = &addr;
                break;
            }
        }
        if (self == nullptr) { // shouldnt happen as long as addr book is initialized
            ERR_PRINT("counldnt find self in address book to make test network");
            return;
        }

        // set up half the nodes to be socket, other half to be shm
        for (auto& [id, addr] : address_book) {
            if (id == self->id) continue;
            addr.kind = RouteKind::Socket;
        }
    }

    void test_network_address_book() {
        PRINT("TEST MODE: making test network address book");

        NodeAddress* self = nullptr;
        for (auto& [id, addr] : address_book) {
            if (addr.kind == RouteKind::Self) {
                self = &addr;
                break;
            }
        }
        if (self == nullptr) { // shouldnt happen as long as addr book is initialized
            ERR_PRINT("counldnt find self in address book to make test network");
            return;
        }

        // set up half the nodes to be socket, other half to be shm
        for (auto& [id, addr] : address_book) {
            if (id == self->id) continue;
            if (self->id % 2 == 0) {
                if (addr.id % 2 == 0) addr.kind = RouteKind::Shm;
                else addr.kind = RouteKind::Socket;
            } else {
                if (addr.id % 2 == 0) addr.kind = RouteKind::Socket;
                else addr.kind = RouteKind::Shm;
            }
        }
    }
}