#pragma once
#include <vector>
#include <string>
constexpr int NUM_NODES = 20;
constexpr int MAX_LABELS = 200;

constexpr int MIN_SEND_LABELS = 5;
constexpr int MAX_SEND_LABELS = 10;

constexpr int MIN_RECV_LABELS = 5;
constexpr int MAX_RECV_LABELS = 25;

constexpr int MIN_SEND_RATE = 10;
constexpr int MAX_SEND_RATE = 500;


struct ScenarioSendLabel {
    int id;
    size_t size;
    int send_rate_ms;
};

struct ScenarioRecvLabel {
    int id;
    size_t size;
};

struct NodeIo {
    int id;
    
    std::vector<ScenarioSendLabel> send_labels;
    int send_count;

    std::vector<ScenarioRecvLabel> recv_labels;
    int recv_count;
};

struct TestScenario {
    int seed;
    std::vector<NodeIo> nodes;

    TestScenario(int seed, int num_nodes) : seed(seed), nodes(std::vector<NodeIo>(num_nodes))  {} 
};

TestScenario generate_test_scenario(const int seed);
void write_scenario_to_file(const TestScenario& scenario);