// axis-aligned bounding box with slab-test ray intersection.

#pragma once

#include <glm/glm.hpp>
#include <algorithm>

class BBox {
public:
    BBox();
    BBox(const glm::dvec3& minCorner, const glm::dvec3& maxCorner);

    static BBox empty();

    const glm::dvec3& min() const { return m_min; }
    const glm::dvec3& max() const { return m_max; }

    glm::dvec3 center() const { return 0.5 * (m_min + m_max); }
    glm::dvec3 extent() const { return m_max - m_min; }
    double surfaceArea() const;
    int maxAxis() const;
    bool isEmpty() const { return m_max.x < m_min.x; }

    BBox unionWith(const BBox& other) const;
    BBox unionWith(const glm::dvec3& p) const;

    // slab test; returns true when the ray hits and writes the entry t.
    bool intersect(const glm::dvec3& origin,
                   const glm::dvec3& invDir,
                   double tMin, double tMax,
                   double& tHit) const;

private:
    glm::dvec3 m_min;
    glm::dvec3 m_max;
};
