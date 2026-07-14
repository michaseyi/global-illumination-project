// analytic sphere intersection + area-light sampling (cone-from-reference).

#include "geometry/sphere.h"

#include <glm/glm.hpp>
#include <cmath>

#include "core/constants.h"
#include "core/sampling.h"
#include "core/frame.h"

Sphere::Sphere(const glm::dvec3& center,
               double radius,
               std::shared_ptr<Material> material)
    : m_center(center), m_radius(radius), m_material(std::move(material)) {}

bool Sphere::intersect(const Ray& ray, HitRecord& rec) const {
    glm::dvec3 oc = ray.origin - m_center;
    double a = glm::dot(ray.direction, ray.direction);
    double b = glm::dot(oc, ray.direction);
    double c = glm::dot(oc, oc) - m_radius * m_radius;
    double disc = b * b - a * c;
    if (disc < 0.0) return false;

    double sq = std::sqrt(disc);
    double t = (-b - sq) / a;
    if (t < ray.tMin || t > ray.tMax) {
        t = (-b + sq) / a;
        if (t < ray.tMin || t > ray.tMax) return false;
    }

    rec.t = t;
    rec.position = ray.at(t);
    glm::dvec3 outwardNormal = (rec.position - m_center) / m_radius;
    rec.setFaceNormal(ray.direction, outwardNormal);

    double theta = std::acos(std::max(-1.0, std::min(1.0, outwardNormal.y)));
    double phi   = std::atan2(outwardNormal.z, outwardNormal.x);
    if (phi < 0.0) phi += 2.0 * constants::kPi;
    rec.uv = glm::dvec2(phi / (2.0 * constants::kPi),
                        1.0 - theta / constants::kPi);

    // longitude tangent in world space (sphere centered at any C, y-up).
    double nx = outwardNormal.x;
    double nz = outwardNormal.z;
    double sinT = std::sqrt(nx * nx + nz * nz);
    if (sinT > 1e-8) {
        rec.tangent = glm::dvec3(-nz, 0.0, nx) / sinT;
        rec.hasTangent = true;
    }

    rec.material = m_material;
    rec.areaLight = m_areaLight;
    rec.primitive = this;
    return true;
}

BBox Sphere::bbox() const {
    glm::dvec3 r(m_radius);
    return BBox(m_center - r, m_center + r);
}

double Sphere::area() const {
    return 4.0 * constants::kPi * m_radius * m_radius;
}

ShapeSample Sphere::sample(double u1, double u2) const {
    glm::dvec3 dir = sampling::uniformSphere(u1, u2);
    ShapeSample s;
    s.normal = dir;
    s.position = m_center + m_radius * dir;
    s.pdf = 1.0 / area();
    return s;
}

ShapeSample Sphere::sampleFromRef(const glm::dvec3& ref,
                                  double u1, double u2) const {
    glm::dvec3 toCenter = m_center - ref;
    double dc2 = glm::dot(toCenter, toCenter);
    double r2  = m_radius * m_radius;
    if (dc2 <= r2) {
        return sample(u1, u2);
    }

    double dc = std::sqrt(dc2);
    double cosThetaMax = std::sqrt(std::max(0.0, 1.0 - r2 / dc2));
    double cosTh = (1.0 - u1) + u1 * cosThetaMax;
    double sinTh = std::sqrt(std::max(0.0, 1.0 - cosTh * cosTh));
    double phi = 2.0 * constants::kPi * u2;

    double ds = dc * cosTh -
                std::sqrt(std::max(0.0, r2 - dc2 * sinTh * sinTh));

    glm::dvec3 wcZ = toCenter / dc;
    Frame frame(wcZ);
    glm::dvec3 wi = frame.toWorld(glm::dvec3(sinTh * std::cos(phi),
                                              sinTh * std::sin(phi),
                                              cosTh));

    ShapeSample s;
    s.position = ref + ds * wi;
    glm::dvec3 n = glm::normalize(s.position - m_center);
    s.position = m_center + n * m_radius;
    s.normal = n;
    double solidAnglePdf = 1.0 / (2.0 * constants::kPi * (1.0 - cosThetaMax));
    double cosOnLight = std::max(0.0, glm::dot(n, -wi));
    s.pdf = solidAnglePdf * cosOnLight / (ds * ds);
    if (!(s.pdf > 0.0)) s.pdf = 1.0 / area();
    return s;
}
