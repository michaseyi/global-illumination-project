// Triangle primitive for the minimal starter.
// This allows the project to move toward mesh rendering later.

#pragma once

#include <memory>
#include <glm/glm.hpp>

#include "geometry/primitive.h"
#include "shading/material.h"

class Triangle : public Primitive {
public:
    Triangle(const glm::dvec3& a,
             const glm::dvec3& b,
             const glm::dvec3& c,
             std::shared_ptr<Material> material);

    bool intersect(const Ray& ray, HitRecord& rec) const override;

private:
    glm::dvec3 m_a;
    glm::dvec3 m_b;
    glm::dvec3 m_c;
    std::shared_ptr<Material> m_material;
};
