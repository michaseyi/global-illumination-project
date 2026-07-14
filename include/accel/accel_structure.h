// Common interface for structures that accelerate ray-scene queries.

#pragma once

#include <vector>

#include "core/ray.h"
#include "scene/hit_record.h"

class Primitive;

class AccelStructure {
public:
    virtual ~AccelStructure() = default;

    // take ownership of nothing; prims must outlive the structure.
    virtual void build(const std::vector<Primitive*>& prims) = 0;

    virtual bool intersect(const Ray& ray, HitRecord& rec) const = 0;

    // any-hit query for shadow rays; may be cheaper than closest-hit.
    virtual bool occluded(const Ray& ray) const = 0;
};
