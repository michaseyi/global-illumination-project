// The starter integrator: direct light only, with hard shadows.

#pragma once

#include "render/integrator.h"

class DirectLightingIntegrator : public Integrator {
public:
    DirectLightingIntegrator(int maxDepth, const Color& background);

    Color Li(const Ray& ray, const Scene& scene, int depth) const override;

private:
    // Kept for consistency with recursive integrator interfaces.
    int m_maxDepth;
    Color m_background;
};
