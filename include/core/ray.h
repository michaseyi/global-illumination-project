// Basic ray structure used for all ray-scene queries.
// The ray interval [tMin, tMax] helps avoid self-intersections.

#pragma once

#include <glm/glm.hpp>
#include "core/constants.h"

struct Ray {
    glm::dvec3 origin{0.0, 0.0, 0.0};
    glm::dvec3 direction{0.0, 0.0, -1.0};
    double tMin = constants::kEpsilon;
    double tMax = constants::kInfinity;

    Ray() = default;

    Ray(const glm::dvec3& o,
        const glm::dvec3& d,
        double minT = constants::kEpsilon,
        double maxT = constants::kInfinity)
        : origin(o), direction(d), tMin(minT), tMax(maxT) {}

    glm::dvec3 at(double t) const {
        return origin + t * direction;
    }
};