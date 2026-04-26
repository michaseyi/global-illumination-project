// Integrator computes the radiance carried by a ray.

#pragma once

#include "core/color.h"
#include "core/ray.h"

class Scene;

class Integrator {
public:
    virtual ~Integrator() = default;

    // depth is used by recursive algorithms; the starter direct-lighting integrator
    // keeps it mainly for interface compatibility.
    virtual Color Li(const Ray& ray, const Scene& scene, int depth) const = 0;
};
