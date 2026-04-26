// Stub BVH interface kept in the starter for Direction 1.

#pragma once

#include "accel/accel_structure.h"

class BVH : public AccelStructure {
public:
    bool intersect(const Ray& ray, HitRecord& rec) const override;
};
