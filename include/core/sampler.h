// per-sample source of random or stratified numbers.
//
// one Sampler lives per worker thread. before each camera sample call
// startSample(px, py, index); then draw dimensions in a fixed order with
// get1D()/get2D().
//
// in Stratified mode each dimension is sampled with correlated multi-jittered
// sampling (Kensler 2013): the `spp` samples of a 2D dimension are spread over a
// jittered grid and visited in a per-(pixel,dimension) shuffled order, so every
// dimension - pixel, lens, and each bounce's light/BSDF choice - is well
// stratified and decorrelated from the others. this lowers Monte-Carlo noise at
// equal spp without the high-dimension breakdown of Halton/Sobol-radical bases.

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

class Sampler {
public:
    enum class Mode { Random, Stratified };

    Sampler(Mode mode, int spp);

    void startSample(int px, int py, int index) {
        m_pixelSeed = hash(uint32_t(px) * 73856093u ^ uint32_t(py) * 19349663u);
        m_index = uint32_t(index);
        m_dim = 0;
    }

    double get1D();
    glm::dvec2 get2D();

    static uint32_t hash(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

private:
    Mode m_mode;
    int m_spp;      // total samples per pixel
    int m_grid;     // floor(sqrt(spp)); side of the 2D stratification grid
    uint32_t m_pixelSeed = 0;
    uint32_t m_index = 0;
    uint32_t m_dim = 0;
};
