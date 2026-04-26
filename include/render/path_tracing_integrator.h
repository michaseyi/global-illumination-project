// Stub for later Monte Carlo global illumination work.

#pragma once

#include "render/integrator.h"

class PathTracingIntegrator : public Integrator {
public:
    Color Li(const Ray& ray, const Scene& scene, int depth) const override;
};
