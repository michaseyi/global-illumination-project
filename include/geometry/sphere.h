// Sphere primitive for the minimal starter.
// This is the simplest analytic shape and a good first test object.

#pragma once

#include <memory>
#include <glm/glm.hpp>

#include "geometry/primitive.h"
#include "shading/material.h"

class Sphere : public Primitive {
public:
    Sphere(const glm::dvec3& center,
           double radius,
           std::shared_ptr<Material> material);

    bool intersect(const Ray& ray, HitRecord& rec) const override;

private:
    glm::dvec3 m_center;
    double m_radius;
    std::shared_ptr<Material> m_material;
};
