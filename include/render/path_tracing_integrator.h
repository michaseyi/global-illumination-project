// unidirectional path tracer with next-event estimation, mis (power heuristic)
// and russian-roulette termination.

#pragma once

#include "render/integrator.h"

class PathTracingIntegrator : public Integrator {
public:
    PathTracingIntegrator(int maxDepth = 8,
                          int rrStart = 3,
                          const Color& background = Color(0.0));

    Color Li(const Ray& ray, const Scene& scene,
             Sampler& sampler) const override;

private:
    int m_maxDepth;
    int m_rrStart;
    Color m_background;
};
