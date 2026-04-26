// Sphere intersection implementation.

#include "geometry/sphere.h"

#include <glm/glm.hpp>
#include <cmath>

Sphere::Sphere(const glm::dvec3& center,
               double radius,
               std::shared_ptr<Material> material)
    : m_center(center), m_radius(radius), m_material(std::move(material)) {}

bool Sphere::intersect(const Ray& ray, HitRecord& rec) const {
    glm::dvec3 oc = ray.origin - m_center;

    // TODO: Implement ray-sphere intersection and populate the HitRecord.

    return false;
}
