// integrator computes the radiance carried along a ray.

#pragma once

#include <memory>

#include "core/color.h"
#include "core/ray.h"
#include "shading/environment.h"

class Scene;
class Sampler;

class Integrator {
public:
    virtual ~Integrator() = default;

    // each call is one independent monte-carlo sample. integrators draw all
    // randomness from `sampler` (call get1D()/get2D() in a stable order) so the
    // sampling strategy is decided in one place.
    virtual Color Li(const Ray& ray, const Scene& scene,
                     Sampler& sampler) const = 0;

    // optional image-based environment; overrides the constant background for
    // rays that leave the scene.
    void setEnvironment(std::shared_ptr<EnvironmentMap> env) {
        m_env = std::move(env);
    }

protected:
    // radiance for an escaped ray: the env map when present, else `fallback`
    // (the integrator's constant background color).
    Color escapedRadiance(const glm::dvec3& dir, const Color& fallback) const {
        return m_env ? m_env->radiance(dir) : fallback;
    }

private:
    std::shared_ptr<EnvironmentMap> m_env;
};
