// Minimal bounding box implementation.

#include "accel/bbox.h"

BBox::BBox()
    : m_min(0.0, 0.0, 0.0), m_max(0.0, 0.0, 0.0) {}

BBox::BBox(const glm::dvec3& minCorner, const glm::dvec3& maxCorner)
    : m_min(minCorner), m_max(maxCorner) {}