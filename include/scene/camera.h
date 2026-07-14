// pinhole + optional thin-lens camera.

#pragma once

#include <glm/glm.hpp>

#include "core/ray.h"

class Camera {
public:
    Camera(const glm::dvec3& eye,
           const glm::dvec3& target,
           const glm::dvec3& up,
           double verticalFovDegrees,
           int imageWidth,
           int imageHeight,
           double aperture = 0.0,
           double focusDistance = 1.0);

    // sample coordinates are pixel-space (in [0,w] and [0,h]); lens uniforms
    // (lu, lv) are in [0,1) and ignored when the aperture is zero.
    Ray generateRay(double sampleX, double sampleY,
                    double lu = 0.5, double lv = 0.5) const;

    Ray generateRay(int px, int py) const;

    int imageWidth() const  { return m_imageWidth; }
    int imageHeight() const { return m_imageHeight; }

private:
    glm::dvec3 m_eye;
    glm::dvec3 m_forward;
    glm::dvec3 m_right;
    glm::dvec3 m_up;

    double m_halfHeight;
    double m_halfWidth;

    int m_imageWidth;
    int m_imageHeight;

    double m_lensRadius;
    double m_focusDistance;
};
