// integrator computes the radiance carried along a ray.

#pragma once

#include "core/color.h"
#include "core/ray.h"

class Scene;

class Integrator {
public:
    virtual ~Integrator() = default;

    // each call is one independent monte-carlo sample. integrators may use
    // the thread-local rng (core/random.h) for any internal randomness.
    virtual Color Li(const Ray& ray, const Scene& scene) const = 0;
};
