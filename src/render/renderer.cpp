#include "render/renderer.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "io/image.h"
#include "render/integrator.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "core/random.h"

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

        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&]() {
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
                            double jx = randomDouble();
                            double jy = randomDouble();
                            double lu = randomDouble();
                            double lv = randomDouble();
                            Ray ray = camera.generateRay(x + jx, y + jy, lu, lv);
                            Color c = integrator.Li(ray, scene);
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
