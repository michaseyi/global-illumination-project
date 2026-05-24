// orthonormal tangent frame around a surface normal.
// pbrt's CoordinateSystem with a stable branchless construction (duff et al.).

#pragma once

#include <glm/glm.hpp>
#include <cmath>

struct Frame {
    glm::dvec3 n;
    glm::dvec3 s;
    glm::dvec3 t;

    Frame() : n(0, 0, 1), s(1, 0, 0), t(0, 1, 0) {}

    explicit Frame(const glm::dvec3& normal) : n(normal) {
        double sign = std::copysign(1.0, n.z);
        const double a = -1.0 / (sign + n.z);
        const double b = n.x * n.y * a;
        s = glm::dvec3(1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x);
        t = glm::dvec3(b, sign + n.y * n.y * a, -n.y);
    }

    glm::dvec3 toLocal(const glm::dvec3& v) const {
        return glm::dvec3(glm::dot(v, s), glm::dot(v, t), glm::dot(v, n));
    }

    glm::dvec3 toWorld(const glm::dvec3& v) const {
        return v.x * s + v.y * t + v.z * n;
    }
};

// shorthand accessors when working in tangent space (z is the normal)
inline double cosTheta(const glm::dvec3& w)    { return w.z; }
inline double absCosTheta(const glm::dvec3& w) { return std::abs(w.z); }
inline double cos2Theta(const glm::dvec3& w)   { return w.z * w.z; }
inline double sin2Theta(const glm::dvec3& w)   { return std::max(0.0, 1.0 - cos2Theta(w)); }
inline double sinTheta(const glm::dvec3& w)    { return std::sqrt(sin2Theta(w)); }
inline double tanTheta(const glm::dvec3& w)    { return sinTheta(w) / cosTheta(w); }
inline double tan2Theta(const glm::dvec3& w)   { return sin2Theta(w) / cos2Theta(w); }
inline bool sameHemisphere(const glm::dvec3& a, const glm::dvec3& b) { return a.z * b.z > 0.0; }
