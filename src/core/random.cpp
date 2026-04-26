// Minimal random utility implementations.
// The starter keeps these helpers ready so later stochastic rendering tasks
// can add sampling without reshaping the whole project.

#include "core/random.h"

#include <random>

namespace {

thread_local std::mt19937 rng(std::random_device{}());

}  // namespace

double randomDouble() {
    static thread_local std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(rng);
}

double randomDouble(double minValue, double maxValue) {
    return minValue + (maxValue - minValue) * randomDouble();
}
