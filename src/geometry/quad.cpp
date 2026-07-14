// parallelogram quad intersection + uniform area sampling.
// adapted from peter shirley's "ray tracing - the next week" (the w-vector trick).

#include "geometry/quad.h"

#include <glm/glm.hpp>
#include <cmath>

#include "core/constants.h"

Quad::Quad(const glm::dvec3& origin,
           const glm::dvec3& u,
           const glm::dvec3& v,
           std::shared_ptr<Material> material)
    : m_origin(origin), m_u(u), m_v(v),
      m_material(std::move(material)) {
    glm::dvec3 n = glm::cross(u, v);
    m_area = glm::length(n);
    m_normal = n / m_area;
    m_d = glm::dot(m_normal, origin);
    m_w = n / glm::dot(n, n);
}

bool Quad::intersect(const Ray& ray, HitRecord& rec) const {
    double denom = glm::dot(m_normal, ray.direction);
    if (std::abs(denom) < 1e-12) return false;
    double t = (m_d - glm::dot(m_normal, ray.origin)) / denom;
    if (t < ray.tMin || t > ray.tMax) return false;

    glm::dvec3 p = ray.at(t);
    glm::dvec3 planarHit = p - m_origin;
    double alpha = glm::dot(m_w, glm::cross(planarHit, m_v));
    double beta  = glm::dot(m_w, glm::cross(m_u, planarHit));
    if (alpha < 0.0 || alpha > 1.0 || beta < 0.0 || beta > 1.0) return false;

    rec.t = t;
    rec.position = p;
    rec.setFaceNormal(ray.direction, m_normal);
    rec.uv = glm::dvec2(alpha, beta);
    rec.tangent = glm::normalize(m_u);
    rec.hasTangent = true;
    rec.material = m_material;
    rec.areaLight = m_areaLight;
    rec.primitive = this;
    return true;
}

BBox Quad::bbox() const {
    glm::dvec3 c0 = m_origin;
    glm::dvec3 c1 = m_origin + m_u;
    glm::dvec3 c2 = m_origin + m_v;
    glm::dvec3 c3 = m_origin + m_u + m_v;
    glm::dvec3 lo = glm::min(glm::min(c0, c1), glm::min(c2, c3));
    glm::dvec3 hi = glm::max(glm::max(c0, c1), glm::max(c2, c3));
    const double pad = 1e-9;
    return BBox(lo - glm::dvec3(pad), hi + glm::dvec3(pad));
}

double Quad::area() const { return m_area; }

ShapeSample Quad::sample(double u1, double u2) const {
    ShapeSample s;
    s.position = m_origin + u1 * m_u + u2 * m_v;
    s.normal = m_normal;
    s.uv = glm::dvec2(u1, u2);
    s.pdf = 1.0 / m_area;
    return s;
}
