#pragma once
#include <vector>
#include <random>

namespace seed {
    float random_float(float min, float max, std::mt19937& rng);
    double random_double(double min, double max, std::mt19937& rng);
    int random_int(int min, int max, std::mt19937& rng);
    std::vector<int> random_unique_range(int min, int max, size_t count, std::mt19937& rng);
}

namespace rng {
    float random_float(float min, float max);
    double random_double(double min, double max);
    int random_int(const int min, const int max);
    std::vector<int> random_unique_range(int min, int max, size_t count);
}
