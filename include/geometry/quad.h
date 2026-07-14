// parallelogram quad defined by an origin and two edge vectors.
// supports uniform area sampling for use as an area light.

#pragma once

#include <memory>
#include <glm/glm.hpp>

#include "geometry/primitive.h"
#include "shading/material.h"

class Quad : public Primitive {
public:
    Quad(const glm::dvec3& origin,
         const glm::dvec3& u,
         const glm::dvec3& v,
         std::shared_ptr<Material> material);

    bool intersect(const Ray& ray, HitRecord& rec) const override;
    BBox bbox() const override;

    double area() const override;
    ShapeSample sample(double u1, double u2) const override;

    const glm::dvec3& origin() const { return m_origin; }
    const glm::dvec3& edgeU() const { return m_u; }
    const glm::dvec3& edgeV() const { return m_v; }

private:
    glm::dvec3 m_origin;
    glm::dvec3 m_u;
    glm::dvec3 m_v;
    glm::dvec3 m_normal;
    double m_area;
    glm::dvec3 m_w; // helper for fast uv computation (pbrt's "w" trick)
    double m_d;
    std::shared_ptr<Material> m_material;
};
