// bbox math: union, surface area, robust slab intersect.

#include "accel/bbox.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
}

BBox::BBox()
    : m_min(kInf, kInf, kInf),
      m_max(-kInf, -kInf, -kInf) {}

BBox::BBox(const glm::dvec3& minCorner, const glm::dvec3& maxCorner)
    : m_min(minCorner), m_max(maxCorner) {}

BBox BBox::empty() { return BBox(); }

double BBox::surfaceArea() const {
    if (isEmpty()) return 0.0;
    glm::dvec3 d = extent();
    return 2.0 * (d.x * d.y + d.y * d.z + d.z * d.x);
}

int BBox::maxAxis() const {
    glm::dvec3 d = extent();
    if (d.x > d.y && d.x > d.z) return 0;
    return d.y > d.z ? 1 : 2;
}

BBox BBox::unionWith(const BBox& other) const {
    return BBox(glm::min(m_min, other.m_min), glm::max(m_max, other.m_max));
}

BBox BBox::unionWith(const glm::dvec3& p) const {
    return BBox(glm::min(m_min, p), glm::max(m_max, p));
}

bool BBox::intersect(const glm::dvec3& origin,
                     const glm::dvec3& invDir,
                     double tMin, double tMax,
                     double& tHit) const {
    double t0 = tMin, t1 = tMax;
    for (int a = 0; a < 3; ++a) {
        double inv = invDir[a];
        double tn = (m_min[a] - origin[a]) * inv;
        double tf = (m_max[a] - origin[a]) * inv;
        if (inv < 0.0) std::swap(tn, tf);
        t0 = tn > t0 ? tn : t0;
        t1 = tf < t1 ? tf : t1;
        if (t1 < t0) return false;
    }
    tHit = t0;
    return true;
}
