#include "render/direct_lighting_integrator.h"

#include <algorithm>
#include <glm/glm.hpp>

#include "scene/scene.h"
#include "shading/light.h"
#include "shading/material.h"
#include "scene/hit_record.h"
#include "core/constants.h"
#include "core/sampler.h"

DirectLightingIntegrator::DirectLightingIntegrator(int maxDepth,
                                                   const Color& background)
    : m_maxDepth(maxDepth), m_background(background) {}

Color DirectLightingIntegrator::Li(const Ray& ray, const Scene& scene,
                                   Sampler& sampler) const {
    HitRecord rec;
    if (!scene.intersect(ray, rec))
        return escapedRadiance(ray.direction, m_background);

    Color L(0.0);
    if (rec.areaLight) {
        L = rec.areaLight->L(rec.geometricNormal, -ray.direction);
    } else if (rec.material) {
        L = rec.material->emission(rec);
    }
    if (!rec.material || rec.material->isDelta()) return L;

    glm::dvec3 wo = -ray.direction;
    const auto& lights = scene.lights();
    if (lights.empty()) return L;

    for (const auto& light : lights) {
        glm::dvec2 lu = sampler.get2D();
        LightSample ls = light->sampleLi(rec.position, rec.shadingNormal,
                                         lu.x, lu.y);
        if (ls.pdf <= 0.0) continue;
        glm::dvec3 to = ls.position - rec.position;
        double dist = glm::length(to);
        Ray shadow(rec.position + 1e-4 * rec.shadingNormal,
                   ls.wi, 1e-4, dist - 1e-3);
        if (scene.occluded(shadow)) continue;
        Color f = rec.material->evaluate(rec, wo, ls.wi);
        double cosTheta = std::max(0.0, glm::dot(rec.shadingNormal, ls.wi));
        if (cosTheta <= 0.0) continue;
        L += f * ls.L * cosTheta / ls.pdf;
    }
    return L;
}
