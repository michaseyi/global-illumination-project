// Axis-aligned bounding box type reserved for later acceleration work.

#pragma once

#include <glm/glm.hpp>

class BBox {
public:
    BBox();
    BBox(const glm::dvec3& minCorner, const glm::dvec3& maxCorner);

    const glm::dvec3& min() const { return m_min; }
    const glm::dvec3& max() const { return m_max; }

private:
    glm::dvec3 m_min;
    glm::dvec3 m_max;
};
