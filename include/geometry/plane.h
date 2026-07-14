// infinite plane (kept for legacy demos; excluded from the bvh).

#pragma once

#include <memory>
#include <glm/glm.hpp>

#include "geometry/primitive.h"
#include "shading/material.h"

class Plane : public Primitive {
public:
    Plane(const glm::dvec3& point,
          const glm::dvec3& normal,
          std::shared_ptr<Material> material);

    bool intersect(const Ray& ray, HitRecord& rec) const override;
    BBox bbox() const override;

private:
    glm::dvec3 m_point;
    glm::dvec3 m_normal;
    std::shared_ptr<Material> m_material;
};
