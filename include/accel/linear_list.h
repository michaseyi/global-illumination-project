// brute-force "acceleration" structure: tests every primitive on every query.
// exists as the baseline for the --accel bvh|octree|brute comparison.

#pragma once

#include <vector>

#include "accel/accel_structure.h"

class LinearList : public AccelStructure {
public:
    void build(const std::vector<Primitive*>& prims) override { m_prims = prims; }

    bool intersect(const Ray& ray, HitRecord& rec) const override;
    bool occluded(const Ray& ray) const override;

private:
    std::vector<Primitive*> m_prims;
};
