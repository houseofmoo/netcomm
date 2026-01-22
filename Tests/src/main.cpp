#include "tests.h"

int main(int argc, char** argv) {
    int id = 0;
    if (argc >= 2) {
        id = std::stoi(argv[1]);
    }
    (void)id;

    //generate_specific_scenario(3888, false);
    //generate_and_write_scenarios(20, false);
    
    run_network_sim(id, 3888, false, true);
    //timed_test(id);
    //add_remove_labels_test(id);

   
    return 0;
}