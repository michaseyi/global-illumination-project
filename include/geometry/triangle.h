// triangle primitive (möller-trumbore intersection).
// optional per-vertex shading normals and uvs; with uvs the hit record
// carries an interpolated uv and a tangent vector for normal mapping.

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

    Triangle(const glm::dvec3& a,
             const glm::dvec3& b,
             const glm::dvec3& c,
             const glm::dvec3& na,
             const glm::dvec3& nb,
             const glm::dvec3& nc,
             std::shared_ptr<Material> material);

    // fully-specified: per-vertex normals + uvs (enables normal mapping).
    Triangle(const glm::dvec3& a,
             const glm::dvec3& b,
             const glm::dvec3& c,
             const glm::dvec3& na,
             const glm::dvec3& nb,
             const glm::dvec3& nc,
             const glm::dvec2& uva,
             const glm::dvec2& uvb,
             const glm::dvec2& uvc,
             std::shared_ptr<Material> material);

    bool intersect(const Ray& ray, HitRecord& rec) const override;
    BBox bbox() const override;

    double area() const override;
    ShapeSample sample(double u1, double u2) const override;

private:
    glm::dvec3 m_a, m_b, m_c;
    glm::dvec3 m_na, m_nb, m_nc;
    glm::dvec2 m_uva, m_uvb, m_uvc;
    bool m_hasShadingNormals = false;
    bool m_hasUVs = false;
    std::shared_ptr<Material> m_material;
};
