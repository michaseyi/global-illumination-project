// Light sources seen by the integrator.

#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "core/color.h"

class Light {
public:
    virtual ~Light() = default;
};

class PointLight : public Light {
public:
    PointLight(const glm::dvec3& position,
               const Color& intensityColor,
               double intensity);

    // World-space light position.
    const glm::dvec3& position() const;

    // Scalar intensity multiplied by the light color.
    Color intensity() const;

private:
    glm::dvec3 m_position;
    Color m_intensityColor;
    double m_intensity;
};
