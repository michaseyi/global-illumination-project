// integrator computes the radiance carried along a ray.

#pragma once

#include "core/color.h"
#include "core/ray.h"

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
};
