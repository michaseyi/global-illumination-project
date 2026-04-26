// Common interface for structures that accelerate ray-scene queries.

#pragma once

#include "core/ray.h"
#include "scene/hit_record.h"

class AccelStructure {
public:
    virtual ~AccelStructure() = default;
    virtual bool intersect(const Ray& ray, HitRecord& rec) const = 0;
};
