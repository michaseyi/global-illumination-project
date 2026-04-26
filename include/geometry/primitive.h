// Primitive is the common interface implemented by every renderable shape.

#pragma once

#include "core/ray.h"
#include "scene/hit_record.h"

class BBox;

class Primitive {
public:
    virtual ~Primitive() = default;

    // Return true and fill rec when the ray hits this primitive.
    virtual bool intersect(const Ray& ray, HitRecord& rec) const = 0;

    // Later directions may add bounding boxes or other acceleration helpers here.
};
