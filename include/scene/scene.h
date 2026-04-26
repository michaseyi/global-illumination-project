// Scene stores the current primitives and lights and answers ray queries.

#pragma once

#include <memory>
#include <vector>

#include "geometry/primitive.h"
#include "shading/light.h"

class Scene {
public:
    void addPrimitive(const std::shared_ptr<Primitive>& primitive);
    void addLight(const std::shared_ptr<Light>& light);

    // Find the closest hit, if any.
    bool intersect(const Ray& ray, HitRecord& rec) const;

    // Return true as soon as any primitive blocks the ray.
    bool occluded(const Ray& ray) const;

    const std::vector<std::shared_ptr<Light>>& lights() const;

private:
    std::vector<std::shared_ptr<Primitive>> m_primitives;
    std::vector<std::shared_ptr<Light>> m_lights;

    // Later directions may cache acceleration structures here.
};
