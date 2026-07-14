#include "render/renderer.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "io/image.h"
#include "render/integrator.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "core/sampler.h"

Renderer::Renderer(int samplesPerPixel)
    : m_samplesPerPixel(samplesPerPixel > 0 ? samplesPerPixel : 1) {}

void Renderer::render(const Scene& scene,
                      const Camera& camera,
                      const Integrator& integrator,
                      Image& image) const {
    const int W = image.width();
    const int H = image.height();
    const int tile = m_tileSize;
    const int tilesX = (W + tile - 1) / tile;
    const int tilesY = (H + tile - 1) / tile;
    const int tileCount = tilesX * tilesY;

    int threads = m_threadCount;
    if (threads <= 0) {
        threads = int(std::thread::hardware_concurrency());
        if (threads <= 0) threads = 1;
    }

    for (int pass = 0; pass < m_samplesPerPixel; ++pass) {
        std::atomic<int> next{0};
        std::vector<std::thread> workers;
        workers.reserve(threads);

        const Sampler::Mode mode = m_sampling == Sampling::Stratified
                                       ? Sampler::Mode::Stratified
                                       : Sampler::Mode::Random;
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&]() {
                Sampler sampler(mode, m_samplesPerPixel);
                while (true) {
                    int idx = next.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= tileCount) return;
                    int tx = idx % tilesX;
                    int ty = idx / tilesX;
                    int x0 = tx * tile;
                    int y0 = ty * tile;
                    int x1 = std::min(W, x0 + tile);
                    int y1 = std::min(H, y0 + tile);

                    for (int y = y0; y < y1; ++y) {
                        for (int x = x0; x < x1; ++x) {
                            // dims 0-1: pixel jitter, 2-3: lens; the integrator
                            // continues drawing from dim 4.
                            sampler.startSample(x, y, pass);
                            glm::dvec2 px = sampler.get2D();
                            glm::dvec2 ls = sampler.get2D();
                            Ray ray = camera.generateRay(x + px.x, y + px.y,
                                                         ls.x, ls.y);
                            Color c = integrator.Li(ray, scene, sampler);
                            image.addSample(x, y, c);
                        }
                    }
                }
            });
        }
        for (auto& w : workers) w.join();

        image.setSampleCount(pass + 1);
        image.finalize();
        if (m_passCallback) m_passCallback(pass, pass + 1);
    }
}
