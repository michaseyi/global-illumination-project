// infinite plane.

#include "geometry/plane.h"

#include <glm/glm.hpp>
#include <cmath>

#include "core/constants.h"

Plane::Plane(const glm::dvec3& point,
             const glm::dvec3& normal,
             std::shared_ptr<Material> material)
    : m_point(point),
      m_normal(glm::normalize(normal)),
      m_material(std::move(material)) {}

bool Plane::intersect(const Ray& ray, HitRecord& rec) const {
    double denom = glm::dot(m_normal, ray.direction);
    if (std::abs(denom) < constants::kEpsilon) return false;

    double t = glm::dot(m_point - ray.origin, m_normal) / denom;
    if (t < ray.tMin || t > ray.tMax) return false;

    rec.t = t;
    rec.position = ray.at(t);
    rec.setFaceNormal(ray.direction, m_normal);
    rec.material = m_material;
    rec.uv = glm::dvec2(rec.position.x, rec.position.z);
    rec.areaLight = m_areaLight;
    rec.primitive = this;
    return true;
}

BBox Plane::bbox() const {
    // unbounded; scene treats empty bboxes as "do not insert into the bvh"
    return BBox::empty();
}
