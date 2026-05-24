// renderable shape interface. all shapes can be intersected, bounded,
// and (optionally) sampled as an area light source.

#pragma once

#include <glm/glm.hpp>

#include "core/ray.h"
#include "scene/hit_record.h"
#include "accel/bbox.h"

class AreaLight;

struct ShapeSample {
    glm::dvec3 position{0.0};
    glm::dvec3 normal{0.0, 1.0, 0.0};
    glm::dvec2 uv{0.0};
    double pdf = 0.0; // wrt surface area
};

class Primitive {
public:
    virtual ~Primitive() = default;

    virtual bool intersect(const Ray& ray, HitRecord& rec) const = 0;
    virtual BBox  bbox() const = 0;

    // analytic surface area; 0 if unbounded/unsupported
    virtual double area() const { return 0.0; }

    // uniformly sample a point on the surface (area measure)
    virtual ShapeSample sample(double /*u1*/, double /*u2*/) const {
        return ShapeSample{};
    }

    // sample a point visible from `ref`; default = forward to area sample
    virtual ShapeSample sampleFromRef(const glm::dvec3& ref,
                                      double u1, double u2) const {
        (void)ref;
        return sample(u1, u2);
    }

    AreaLight* areaLight() const { return m_areaLight; }
    void setAreaLight(AreaLight* light) { m_areaLight = light; }

protected:
    AreaLight* m_areaLight = nullptr;
};
