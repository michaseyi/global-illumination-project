#include "render/whitted_integrator.h"

#include <algorithm>
#include <glm/glm.hpp>

#include "scene/scene.h"
#include "shading/light.h"
#include "shading/material.h"
#include "scene/hit_record.h"
#include "core/constants.h"
#include "core/random.h"

WhittedIntegrator::WhittedIntegrator(int maxDepth, const Color& background)
    : m_maxDepth(maxDepth), m_background(background) {}

Color WhittedIntegrator::Li(const Ray& ray, const Scene& scene) const {
    return Li_rec(ray, scene, 0);
}

Color WhittedIntegrator::Li_rec(const Ray& ray, const Scene& scene, int depth) const {
    HitRecord rec;
    if (!scene.intersect(ray, rec)) return m_background;

    Color L(0.0);
    if (rec.areaLight) {
        L = rec.areaLight->L(rec.geometricNormal, -ray.direction);
    } else if (rec.material) {
        L = rec.material->emission(rec);
    }
    if (!rec.material) return L;

    glm::dvec3 wo = -ray.direction;

    if (!rec.material->isDelta()) {
        for (const auto& light : scene.lights()) {
            LightSample ls = light->sampleLi(rec.position, rec.shadingNormal,
                                             randomDouble(), randomDouble());
            if (ls.pdf <= 0.0) continue;
            double dist = ls.distance;
            Ray shadow(rec.position + 1e-4 * rec.shadingNormal,
                       ls.wi, 1e-4, dist - 1e-3);
            if (scene.occluded(shadow)) continue;
            Color f = rec.material->evaluate(rec, wo, ls.wi);
            double cosTheta = std::max(0.0, glm::dot(rec.shadingNormal, ls.wi));
            if (cosTheta <= 0.0) continue;
            L += f * ls.L * cosTheta / ls.pdf;
        }
    }

    if (depth + 1 >= m_maxDepth) return L;

    if (rec.material->isDelta()) {
        glm::dvec3 u(randomDouble(), randomDouble(), randomDouble());
        MaterialSample ms = rec.material->sample(rec, wo, u);
        if (ms.valid && ms.pdf > 0.0) {
            Ray next(rec.position + 1e-4 *
                     ((glm::dot(ms.wi, rec.geometricNormal) > 0.0)
                        ? rec.geometricNormal : -rec.geometricNormal),
                     ms.wi);
            L += ms.weight * Li_rec(next, scene, depth + 1);
        }
    }
    return L;
}
