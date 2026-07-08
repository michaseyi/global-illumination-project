// wo / wi convention follows pbrt: both point away from the surface.

#include "render/path_tracing_integrator.h"

#include <algorithm>
#include <glm/glm.hpp>
#include <cmath>

#include "scene/scene.h"
#include "shading/light.h"
#include "shading/material.h"
#include "scene/hit_record.h"
#include "geometry/primitive.h"
#include "core/constants.h"
#include "core/sampler.h"
#include "core/sampling.h"

namespace {

// area pdf for sampling a particular emissive primitive from `ref` and direction
// `wi` that lands on `hit`. converted to solid-angle measure at `ref`.
double areaPdfToSolidAngle(const HitRecord& hit, const glm::dvec3& ref) {
    if (!hit.primitive) return 0.0;
    double area = hit.primitive->area();
    if (area <= 0.0) return 0.0;
    glm::dvec3 to = hit.position - ref;
    double d2 = glm::dot(to, to);
    glm::dvec3 wi = to / std::sqrt(d2);
    double cosOnLight = std::abs(glm::dot(hit.geometricNormal, -wi));
    if (cosOnLight <= 0.0) return 0.0;
    return d2 / (area * cosOnLight);
}

bool isBlack(const Color& c) { return c.r <= 0.0 && c.g <= 0.0 && c.b <= 0.0; }

} // namespace

PathTracingIntegrator::PathTracingIntegrator(int maxDepth, int rrStart,
                                             const Color& background)
    : m_maxDepth(maxDepth), m_rrStart(rrStart), m_background(background) {}

Color PathTracingIntegrator::Li(const Ray& primaryRay, const Scene& scene,
                                Sampler& sampler) const {
    Color L(0.0);
    Color beta(1.0);
    Ray ray = primaryRay;

    bool prevSpecular = true;   // counts emission at primary hit fully
    double prevBsdfPdf = 1.0;

    const auto& lights = scene.lights();
    int numLights = int(lights.size());

    for (int bounces = 0; bounces < m_maxDepth; ++bounces) {
        HitRecord rec;
        bool hit = scene.intersect(ray, rec);

        if (!hit) {
            if (prevSpecular || bounces == 0) {
                L += beta * m_background;
            }
            break;
        }

        // prefer the bound area light when present (handles two-sidedness).
        Color emitted(0.0);
        if (rec.areaLight) {
            emitted = rec.areaLight->L(rec.geometricNormal, -ray.direction);
        } else if (rec.material) {
            emitted = rec.material->emission(rec);
        }

        if (!isBlack(emitted)) {
            if (bounces == 0 || prevSpecular || numLights == 0) {
                L += beta * emitted;
            } else {
                double lightPickPdf = 1.0 / double(numLights);
                double lightPdfSA = areaPdfToSolidAngle(rec, ray.origin)
                                  * lightPickPdf;
                double w = sampling::powerHeuristic(prevBsdfPdf, lightPdfSA);
                L += beta * emitted * w;
            }
        }

        if (!rec.material) break;

        glm::dvec3 wo = -ray.direction;
        bool surfaceIsDelta = rec.material->isDelta();

        // next-event estimation
        if (!surfaceIsDelta && numLights > 0) {
            int li = std::min(numLights - 1,
                              int(sampler.get1D() * double(numLights)));
            const auto& light = lights[li];
            double lightPickPdf = 1.0 / double(numLights);

            glm::dvec2 lu = sampler.get2D();
            LightSample ls = light->sampleLi(rec.position, rec.shadingNormal,
                                             lu.x, lu.y);
            if (ls.pdf > 0.0 && !isBlack(ls.L)) {
                Ray shadow(rec.position +
                           1e-4 * (glm::dot(ls.wi, rec.geometricNormal) > 0.0
                               ? rec.geometricNormal : -rec.geometricNormal),
                           ls.wi, 1e-4, ls.distance - 1e-3);
                if (!scene.occluded(shadow)) {
                    Color f = rec.material->evaluate(rec, wo, ls.wi);
                    double cosTheta = std::max(0.0,
                                       glm::dot(rec.shadingNormal, ls.wi));
                    if (cosTheta > 0.0 && !isBlack(f)) {
                        double lightPdfSA = ls.pdf * lightPickPdf;
                        if (light->isDelta()) {
                            L += beta * f * ls.L * cosTheta / lightPdfSA;
                        } else {
                            double bsdfPdf = rec.material->pdf(rec, wo, ls.wi);
                            double w = sampling::powerHeuristic(lightPdfSA, bsdfPdf);
                            L += beta * f * ls.L * cosTheta * w / lightPdfSA;
                        }
                    }
                }
            }
        }

        // bsdf sample
        glm::dvec2 uxy = sampler.get2D();
        glm::dvec3 u(uxy.x, uxy.y, sampler.get1D());
        MaterialSample ms = rec.material->sample(rec, wo, u);
        if (!ms.valid || ms.pdf <= 0.0 || isBlack(ms.weight)) break;

        beta *= ms.weight;
        prevSpecular = ms.delta;
        prevBsdfPdf  = ms.delta ? 0.0 : ms.pdf;

        glm::dvec3 offsetN = glm::dot(ms.wi, rec.geometricNormal) > 0.0
                             ? rec.geometricNormal : -rec.geometricNormal;
        ray = Ray(rec.position + 1e-4 * offsetN, ms.wi);

        if (bounces >= m_rrStart) {
            double q = std::max(0.05, 1.0 - std::max({beta.r, beta.g, beta.b}));
            if (sampler.get1D() < q) break;
            beta /= (1.0 - q);
        }

        if (isBlack(beta)) break;
    }

    return L;
}
