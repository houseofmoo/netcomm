#include "randomizer/randomizer.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace seed {
    // for a given seed, return the same "random" numbers
    // if call order of random functions differs per run, we wont get the same
    // random numbers since the generator has state that is mutated each time
    // we call one of these functions

    // std::mt19937 rng(12345); // 12345 can be any value
    // thread_local std::mt19937 rng(123); to keep the generator local to that thread

    float random_float(float min, float max, std::mt19937& rng) {
        // [min, max)
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }

    double random_double(double min, double max, std::mt19937& rng) {
        // [min, max)
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }

    int random_int(int min, int max, std::mt19937& rng) {
        // [min, max] inclusive
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    std::vector<int> random_unique_range(int min, int max, size_t count, std::mt19937& rng) {
        const size_t range_size = static_cast<size_t>(max - min + 1);
        if (count > range_size)
            throw std::runtime_error("count exceeds range");

        if (count > 10000)
            throw std::runtime_error("range exceeds 10,000, lets keep it reasonable");

        std::vector<int> values(range_size);
        std::iota(values.begin(), values.end(), min);

        std::shuffle(values.begin(), values.end(), rng);

        values.resize(count);
        return values;
    }
}

namespace rng {
    float random_float(float min, float max) {
        // [min, max) excludes max
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }

    double random_double(double min, double max) {
        
        // [min, max) excludes max
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }

    int random_int(const int min, const int max) {
        // [min, max] inclusive
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    std::vector<int> random_unique_range(int min, int max, size_t count) {
        const size_t range_size = static_cast<size_t>(max - min + 1);
        if (count > range_size) {
            throw std::runtime_error("count exceeds range");
        }

        if (count > 10000) {
            throw std::runtime_error("range exceeds 10,000, lets keep it reasonable");
        }

        // generate a ascending list from min to max
        std::vector<int> values(range_size);
        std::iota(values.begin(), values.end(), min);

        // randomly shuffle that list
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::shuffle(values.begin(), values.end(), rng);

        // chop off excess values
        values.resize(count);
        return values;
    }
}
