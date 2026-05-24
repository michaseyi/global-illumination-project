// whitted-style integrator: nee for diffuse hits + recursive specular bounces.

#pragma once

#include "render/integrator.h"

class WhittedIntegrator : public Integrator {
public:
    WhittedIntegrator(int maxDepth = 5, const Color& background = Color(0.0));

    Color Li(const Ray& ray, const Scene& scene) const override;

private:
    Color Li_rec(const Ray& ray, const Scene& scene, int depth) const;

    int m_maxDepth;
    Color m_background;
};
