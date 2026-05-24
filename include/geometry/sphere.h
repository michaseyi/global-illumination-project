// analytic sphere primitive.

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
    BBox bbox() const override;

    double area() const override;
    ShapeSample sample(double u1, double u2) const override;
    ShapeSample sampleFromRef(const glm::dvec3& ref,
                              double u1, double u2) const override;

    const glm::dvec3& center() const { return m_center; }
    double radius() const { return m_radius; }

private:
    glm::dvec3 m_center;
    double m_radius;
    std::shared_ptr<Material> m_material;
};
