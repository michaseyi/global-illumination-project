// Simple pinhole camera used to generate primary rays.

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
           int imageHeight);

    // Generate a ray through an arbitrary sample location inside the image.
    Ray generateRay(double sampleX, double sampleY) const;

    // Convenience overload for the pixel center.
    Ray generateRay(int px, int py) const;

    int imageWidth() const { return m_imageWidth; }
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
};
