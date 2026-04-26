// Direct illumination integrator with hard shadows.

#include "render/direct_lighting_integrator.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <memory>

#include "scene/scene.h"
#include "shading/light.h"
#include "shading/material.h"
#include "scene/hit_record.h"
#include "core/constants.h"

DirectLightingIntegrator::DirectLightingIntegrator(int maxDepth,
                                                   const Color& background)
    : m_maxDepth(maxDepth), m_background(background) {}

Color DirectLightingIntegrator::Li(const Ray& ray,
                                   const Scene& scene,
                                   int depth) const {
    if (depth > m_maxDepth) {
        return Color(0.0, 0.0, 0.0);
    }

    HitRecord rec;

    // If the ray misses the scene, return the background color.
    if (!scene.intersect(ray, rec)) {
        return m_background;
    }

    Color result = rec.material ? rec.material->emission(rec) : Color(0.0, 0.0, 0.0);

    // wo is the outgoing direction toward the camera/viewer.
    glm::dvec3 wo = -ray.direction;

    // Traverse all lights in the scene.
    for (const auto& lightBase : scene.lights()) {
        // The starter scene only uses point lights.
        auto pointLight = std::dynamic_pointer_cast<PointLight>(lightBase);
        if (!pointLight) {
            continue;
        }

        // TODO:        
        // Compute the contribution of this light source:
        // determine the light direction, test visibility with a shadow ray,
        // evaluate the material response, and accumulate the result.
    }

    return result;
}
