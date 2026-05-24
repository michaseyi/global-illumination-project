// triangle: möller-trumbore intersect, bbox, uniform area sampling,
// optional shading normals, optional uvs + tangent for normal mapping.

#include "geometry/triangle.h"

#include <glm/glm.hpp>
#include <cmath>

#include "core/constants.h"

Triangle::Triangle(const glm::dvec3& a,
                   const glm::dvec3& b,
                   const glm::dvec3& c,
                   std::shared_ptr<Material> material)
    : m_a(a), m_b(b), m_c(c),
      m_na(0.0), m_nb(0.0), m_nc(0.0),
      m_uva(0.0), m_uvb(0.0), m_uvc(0.0),
      m_material(std::move(material)) {}

Triangle::Triangle(const glm::dvec3& a,
                   const glm::dvec3& b,
                   const glm::dvec3& c,
                   const glm::dvec3& na,
                   const glm::dvec3& nb,
                   const glm::dvec3& nc,
                   std::shared_ptr<Material> material)
    : m_a(a), m_b(b), m_c(c),
      m_na(glm::normalize(na)),
      m_nb(glm::normalize(nb)),
      m_nc(glm::normalize(nc)),
      m_uva(0.0), m_uvb(0.0), m_uvc(0.0),
      m_hasShadingNormals(true),
      m_material(std::move(material)) {}

Triangle::Triangle(const glm::dvec3& a,
                   const glm::dvec3& b,
                   const glm::dvec3& c,
                   const glm::dvec3& na,
                   const glm::dvec3& nb,
                   const glm::dvec3& nc,
                   const glm::dvec2& uva,
                   const glm::dvec2& uvb,
                   const glm::dvec2& uvc,
                   std::shared_ptr<Material> material)
    : m_a(a), m_b(b), m_c(c),
      m_na(glm::normalize(na)),
      m_nb(glm::normalize(nb)),
      m_nc(glm::normalize(nc)),
      m_uva(uva), m_uvb(uvb), m_uvc(uvc),
      m_hasShadingNormals(true),
      m_hasUVs(true),
      m_material(std::move(material)) {}

bool Triangle::intersect(const Ray& ray, HitRecord& rec) const {
    glm::dvec3 edge1 = m_b - m_a;
    glm::dvec3 edge2 = m_c - m_a;
    glm::dvec3 pvec  = glm::cross(ray.direction, edge2);
    double det = glm::dot(edge1, pvec);
    if (std::abs(det) < 1e-12) return false;

    double invDet = 1.0 / det;
    glm::dvec3 tvec = ray.origin - m_a;
    double u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    glm::dvec3 qvec = glm::cross(tvec, edge1);
    double v = glm::dot(ray.direction, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    double t = glm::dot(edge2, qvec) * invDet;
    if (t < ray.tMin || t > ray.tMax) return false;

    rec.t = t;
    rec.position = ray.at(t);
    glm::dvec3 outwardNormal = glm::normalize(glm::cross(edge1, edge2));
    rec.setFaceNormal(ray.direction, outwardNormal);

    double w = 1.0 - u - v;
    if (m_hasShadingNormals) {
        glm::dvec3 ns = glm::normalize(w * m_na + u * m_nb + v * m_nc);
        if (!rec.frontFace) ns = -ns;
        rec.shadingNormal = ns;
    }

    if (m_hasUVs) {
        glm::dvec2 uvHit = w * m_uva + u * m_uvb + v * m_uvc;
        rec.uv = uvHit;

        // tangent from edge/uv deltas
        glm::dvec2 duv1 = m_uvb - m_uva;
        glm::dvec2 duv2 = m_uvc - m_uva;
        double denom = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(denom) > 1e-20) {
            glm::dvec3 T = (edge1 * duv2.y - edge2 * duv1.y) / denom;
            // orthonormalize against the shading normal (gram-schmidt)
            T = glm::normalize(T - rec.shadingNormal *
                                   glm::dot(rec.shadingNormal, T));
            rec.tangent = T;
            rec.hasTangent = true;
        }
    } else {
        rec.uv = glm::dvec2(u, v);
    }

    rec.material = m_material;
    rec.areaLight = m_areaLight;
    rec.primitive = this;
    return true;
}

BBox Triangle::bbox() const {
    glm::dvec3 lo = glm::min(m_a, glm::min(m_b, m_c));
    glm::dvec3 hi = glm::max(m_a, glm::max(m_b, m_c));
    const double pad = 1e-9;
    return BBox(lo - glm::dvec3(pad), hi + glm::dvec3(pad));
}

double Triangle::area() const {
    return 0.5 * glm::length(glm::cross(m_b - m_a, m_c - m_a));
}

ShapeSample Triangle::sample(double u1, double u2) const {
    double su = std::sqrt(u1);
    double b0 = 1.0 - su;
    double b1 = u2 * su;
    double b2 = 1.0 - b0 - b1;
    ShapeSample s;
    s.position = b0 * m_a + b1 * m_b + b2 * m_c;
    glm::dvec3 n = glm::normalize(glm::cross(m_b - m_a, m_c - m_a));
    s.normal = m_hasShadingNormals
        ? glm::normalize(b0 * m_na + b1 * m_nb + b2 * m_nc)
        : n;
    s.uv = glm::dvec2(b1, b2);
    s.pdf = 1.0 / area();
    return s;
}
