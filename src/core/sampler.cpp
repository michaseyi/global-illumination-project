#include "core/sampler.h"

#include <cmath>

#include "core/random.h"

namespace {

// Kensler's permutation: a bijection of [0, l) seeded by p, computed in constant
// space. used to shuffle which stratum each sample index maps to, per pixel and
// per dimension, so dimensions decorrelate. (Kensler, "Correlated Multi-Jittered
// Sampling", Pixar TM-13-01, 2013.)
uint32_t permute(uint32_t i, uint32_t l, uint32_t p) {
    if (l <= 1) return 0;
    uint32_t w = l - 1;
    w |= w >> 1; w |= w >> 2; w |= w >> 4; w |= w >> 8; w |= w >> 16;
    do {
        i ^= p;              i *= 0xe170893du;
        i ^= p >> 16;
        i ^= (i & w) >> 4;   i ^= p >> 8;   i *= 0x0929eb3fu;
        i ^= p >> 23;        i ^= (i & w) >> 1;  i *= 1u | p >> 27;
        i *= 0x6935fa69u;    i ^= (i & w) >> 11; i *= 0x74dcb303u;
        i ^= (i & w) >> 2;   i *= 0x9e501cc3u;   i ^= (i & w) >> 2;
        i *= 0xc860a3dfu;    i &= w;             i ^= i >> 5;
    } while (i >= l);
    return (i + p) % l;
}

}  // namespace

Sampler::Sampler(Mode mode, int spp)
    : m_mode(mode), m_spp(spp > 0 ? spp : 1) {
    m_grid = int(std::sqrt(double(m_spp)));
    if (m_grid < 1) m_grid = 1;
}

double Sampler::get1D() {
    uint32_t dim = m_dim++;
    if (m_mode == Mode::Random) return randomDouble();
    // stratify [0,1) into spp shuffled cells.
    uint32_t seed = hash(m_pixelSeed ^ (dim * 0x9e3779b9u + 0x1234567u));
    uint32_t cell = permute(m_index % uint32_t(m_spp), uint32_t(m_spp), seed);
    return (cell + randomDouble()) / m_spp;
}

glm::dvec2 Sampler::get2D() {
    uint32_t dim = m_dim++;
    int n = m_grid * m_grid;
    if (m_mode == Mode::Random || m_index >= uint32_t(n)) {
        return glm::dvec2(randomDouble(), randomDouble());
    }
    // shuffle which of the grid*grid cells this sample lands in, per pixel+dim.
    uint32_t seed = hash(m_pixelSeed ^ (dim * 0x85ebca6bu + 0x9e3779b9u));
    uint32_t cell = permute(m_index, uint32_t(n), seed);
    int sx = cell % m_grid, sy = cell / m_grid;
    return glm::dvec2((sx + randomDouble()) / m_grid,
                      (sy + randomDouble()) / m_grid);
}
