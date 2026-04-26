// Starter stub for a later Whitted-style integrator.
// Returning black keeps the executable buildable before the module is implemented.

#include "render/whitted_integrator.h"
#include "scene/scene.h"

Color WhittedIntegrator::Li(const Ray&, const Scene&, int) const {
    return Color(0.0, 0.0, 0.0);
}
