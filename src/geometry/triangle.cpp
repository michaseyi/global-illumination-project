// Triangle intersection implementation using the Moller-Trumbore algorithm.

#include "geometry/triangle.h"

#include <glm/glm.hpp>
#include <cmath>

#include "core/constants.h"

Triangle::Triangle(const glm::dvec3& a,
                   const glm::dvec3& b,
                   const glm::dvec3& c,
                   std::shared_ptr<Material> material)
    : m_a(a), m_b(b), m_c(c), m_material(std::move(material)) {}

bool Triangle::intersect(const Ray& ray, HitRecord& rec) const {
    glm::dvec3 edge1 = m_b - m_a;
    glm::dvec3 edge2 = m_c - m_a;

    glm::dvec3 pvec = glm::cross(ray.direction, edge2);
    double det = glm::dot(edge1, pvec);

    if (std::abs(det) < constants::kEpsilon) {
        return false;
    }

    double invDet = 1.0 / det;
    glm::dvec3 tvec = ray.origin - m_a;

    double u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) {
        return false;
    }

    glm::dvec3 qvec = glm::cross(tvec, edge1);
    double v = glm::dot(ray.direction, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    double t = glm::dot(edge2, qvec) * invDet;
    if (t < ray.tMin || t > ray.tMax) {
        return false;
    }

    rec.t = t;
    rec.position = ray.at(t);

    glm::dvec3 outwardNormal = glm::normalize(glm::cross(edge1, edge2));
    rec.setFaceNormal(ray.direction, outwardNormal);
    rec.material = m_material;
    rec.uv = glm::dvec2(u, v);

    return true;
}
