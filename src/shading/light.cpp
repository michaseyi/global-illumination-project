// point light + diffuse area light implementations.

#include "shading/light.h"

#include <algorithm>
#include <cmath>

#include "geometry/primitive.h"
#include "scene/hit_record.h"
#include "core/constants.h"


PointLight::PointLight(const glm::dvec3& position,
                       const Color& intensityColor,
                       double intensity)
    : m_position(position),
      m_intensityColor(intensityColor),
      m_intensity(intensity) {}

const glm::dvec3& PointLight::position() const { return m_position; }

Color PointLight::intensity() const { return m_intensity * m_intensityColor; }

LightSample PointLight::sampleLi(const glm::dvec3& ref,
                                 const glm::dvec3& /*refNormal*/,
                                 double /*u1*/, double /*u2*/) const {
    LightSample s;
    glm::dvec3 diff = m_position - ref;
    double d2 = glm::dot(diff, diff);
    double d = std::sqrt(d2);
    s.position = m_position;
    s.wi = diff / d;
    s.distance = d;
    s.L = intensity() / d2;
    s.pdf = 1.0;
    s.isDelta = true;
    s.normal = -s.wi;
    return s;
}

AreaLight::AreaLight(std::shared_ptr<Primitive> shape, const Color& emission,
                     bool twoSided)
    : m_shape(std::move(shape)), m_emission(emission), m_twoSided(twoSided) {}

LightSample AreaLight::sampleLi(const glm::dvec3& ref,
                                const glm::dvec3& /*refNormal*/,
                                double u1, double u2) const {
    LightSample s;
    ShapeSample ps = m_shape->sampleFromRef(ref, u1, u2);
    glm::dvec3 diff = ps.position - ref;
    double d2 = glm::dot(diff, diff);
    if (d2 <= 0.0) return s;
    double d = std::sqrt(d2);
    glm::dvec3 wi = diff / d;
    double cosOnLight = glm::dot(ps.normal, -wi);
    if (!m_twoSided && cosOnLight <= 0.0) return s;
    if (m_twoSided) cosOnLight = std::abs(cosOnLight);

    s.position = ps.position;
    s.normal = ps.normal;
    s.wi = wi;
    s.distance = d;
    s.L = m_emission;
    // convert area-pdf to solid-angle-pdf at the reference point
    if (ps.pdf > 0.0 && cosOnLight > 0.0) {
        s.pdf = ps.pdf * d2 / cosOnLight;
    }
    s.isDelta = false;
    return s;
}

double AreaLight::pdfLi(const glm::dvec3& /*ref*/,
                        const glm::dvec3& /*wi*/) const {
    // returned in area measure; callers convert via d^2 / |cosOnLight|.
    double area = m_shape->area();
    return area > 0.0 ? 1.0 / area : 0.0;
}

Color AreaLight::L(const glm::dvec3& n, const glm::dvec3& w) const {
    if (m_twoSided) return m_emission;
    return glm::dot(n, w) > 0.0 ? m_emission : Color(0.0);
}
