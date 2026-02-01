#include "tests.h"
#include <fstream>
#include <string>
#include <sstream>

int get_node_count() {
    // cound number of nodes in peer_ips.cfg that are active
    std::string path = "etc/peer_ips.cfg";
     std::ifstream file(path);
    if (!file.is_open()) {
        ERR_PRINT("could not open file ", path);
        return 0;
    }

    int num_nodes = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> columns;
        std::stringstream ss(line);
        std::string cell;

        if (line.empty() || line.rfind("#", 0) == 0) continue;

        num_nodes += 1;
    }

    return num_nodes;
}

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }
    (void)argc;
    (void)argv;
    (void)id;

    int num_nodes = get_node_count();
    (void)num_nodes;

    //generate_specific_scenario(3888, num_nodes, false);
    //generate_and_write_scenarios(20, num_nodes, false);
    
    //run_network_sim(id, 3888, num_nodes, false, false);
    //timed_test(id);
    //add_remove_labels_test(id);

    small_test(id);

    return 0;
}

