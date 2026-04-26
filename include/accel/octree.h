// Stub octree interface kept in the starter for future experiments.

#pragma once

#include "accel/accel_structure.h"

class Octree : public AccelStructure {
public:
    bool intersect(const Ray& ray, HitRecord& rec) const override;
};
