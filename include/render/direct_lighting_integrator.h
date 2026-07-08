// direct-lighting integrator: nee against all lights (point + area).
// no indirect bounces; emission is added at the primary hit.

#pragma once

#include "render/integrator.h"

class DirectLightingIntegrator : public Integrator {
public:
    DirectLightingIntegrator(int maxDepth, const Color& background);

    Color Li(const Ray& ray, const Scene& scene,
             Sampler& sampler) const override;

private:
    int m_maxDepth;          // unused but kept for backward compatibility
    Color m_background;
};
