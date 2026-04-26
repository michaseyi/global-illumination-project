// Stub for later reflection/refraction work.

#pragma once

#include "render/integrator.h"

class WhittedIntegrator : public Integrator {
public:
    Color Li(const Ray& ray, const Scene& scene, int depth) const override;
};
