// Minimal renderer implementation.
// The starter still performs one sample per pixel and delegates all radiance
// computation to the selected integrator. The samples-per-pixel parameter is
// kept only as a forward-compatible hook for later assignments.

#include "render/renderer.h"

#include "io/image.h"
#include "render/integrator.h"
#include "scene/camera.h"
#include "scene/scene.h"

Renderer::Renderer(int samplesPerPixel)
    : m_samplesPerPixel(samplesPerPixel > 0 ? samplesPerPixel : 1) {}

void Renderer::render(const Scene& scene,
                      const Camera& camera,
                      const Integrator& integrator,
                      Image& image) const {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            Ray ray = camera.generateRay(x, y);
            Color color = integrator.Li(ray, scene, 0);
            image.setPixel(x, y, color);
        }
    }
}
