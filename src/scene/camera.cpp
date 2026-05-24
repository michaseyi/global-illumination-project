// pinhole + thin-lens primary ray generation.

#include "scene/camera.h"

#include <cmath>
#include <glm/glm.hpp>

#include "core/constants.h"
#include "core/sampling.h"

Camera::Camera(const glm::dvec3& eye,
               const glm::dvec3& target,
               const glm::dvec3& up,
               double verticalFovDegrees,
               int imageWidth,
               int imageHeight,
               double aperture,
               double focusDistance)
    : m_eye(eye),
      m_imageWidth(imageWidth),
      m_imageHeight(imageHeight),
      m_lensRadius(0.5 * aperture),
      m_focusDistance(focusDistance > 0.0 ? focusDistance : 1.0) {
    m_forward = glm::normalize(target - eye);
    m_right = glm::normalize(glm::cross(m_forward, up));
    m_up = glm::normalize(glm::cross(m_right, m_forward));

    const double aspect = static_cast<double>(imageWidth) / imageHeight;
    const double theta = verticalFovDegrees * constants::kPi / 180.0;
    m_halfHeight = std::tan(theta * 0.5);
    m_halfWidth = aspect * m_halfHeight;
}

Ray Camera::generateRay(double sampleX, double sampleY,
                        double lu, double lv) const {
    // ndc with +y up, image y axis points down
    double u = (2.0 * sampleX / m_imageWidth)  - 1.0;
    double v = 1.0 - (2.0 * sampleY / m_imageHeight);

    glm::dvec3 dir = glm::normalize(
        m_forward + u * m_halfWidth * m_right + v * m_halfHeight * m_up);

    if (m_lensRadius <= 0.0) {
        return Ray(m_eye, dir);
    }

    // thin-lens: jitter the origin on a disk, aim through the focal point.
    glm::dvec2 disk = sampling::concentricDisk(lu, lv) * m_lensRadius;
    glm::dvec3 originOffset = disk.x * m_right + disk.y * m_up;
    glm::dvec3 origin = m_eye + originOffset;
    glm::dvec3 focal  = m_eye + dir * (m_focusDistance / glm::dot(dir, m_forward));
    return Ray(origin, glm::normalize(focal - origin));
}

Ray Camera::generateRay(int px, int py) const {
    return generateRay(px + 0.5, py + 0.5);
}
