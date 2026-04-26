// Starter stub for a later path tracer.
// Returning black keeps the project buildable before the module is implemented.

#include "render/path_tracing_integrator.h"
#include "scene/scene.h"

Color PathTracingIntegrator::Li(const Ray&, const Scene&, int) const {
    return Color(0.0, 0.0, 0.0);
}
