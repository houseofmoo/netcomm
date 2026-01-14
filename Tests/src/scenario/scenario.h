#pragma once
#include <vector>

constexpr size_t KILOBYTE = 1024;
constexpr int NUM_NODES = 20;
constexpr int MAX_LABELS = 200;
constexpr int MAX_SEND_LABELS = 10;
constexpr int MAX_RECV_LABELS = 25;

struct ScenarioLabel {
    int id;
    size_t size;
    bool is_sent;
};

struct ScenarioSendLabel {
    int id;
    size_t size;
    int send_rate_ms;
};

struct ScenarioRecvLabel {
    int id;
    size_t size;
};

struct NodeScenario {
    int id;
    std::vector<ScenarioSendLabel> send_labels;
    std::vector<ScenarioRecvLabel> recv_labels;
};

std::vector<NodeScenario> generate_test_scenario();
void write_to_file(const char*, const std::vector<NodeScenario>& scenarios);