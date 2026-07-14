// scene stores primitives and lights, builds an acceleration structure on
// demand, and answers ray queries.

#pragma once

#include <memory>
#include <vector>

#include "geometry/primitive.h"
#include "shading/light.h"
#include "accel/accel_structure.h"

class AreaLight;

class Scene {
public:
    // which structure answers ray queries over the bounded primitives.
    enum class AccelType { BVH, Octree, Linear };

    Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    void addPrimitive(const std::shared_ptr<Primitive>& primitive);
    void addLight(const std::shared_ptr<Light>& light);

    // bind an emissive primitive + diffuse area light. light is appended to
    // the scene's light list and to emissivePrimitives() for nee.
    void addAreaLight(const std::shared_ptr<Primitive>& shape,
                      const std::shared_ptr<AreaLight>& light);

    // build the acceleration structure from currently registered primitives.
    // call once after the scene is fully populated and before any rendering.
    void build();

    // switch acceleration structure; rebuilds immediately when the scene was
    // already built (scene factories/loaders build with the default bvh).
    void setAccel(AccelType type);
    AccelType accelType() const { return m_accelType; }

    bool intersect(const Ray& ray, HitRecord& rec) const;
    bool occluded(const Ray& ray) const;

    const std::vector<std::shared_ptr<Light>>& lights() const { return m_lights; }
    const std::vector<std::shared_ptr<Primitive>>& emissivePrimitives() const {
        return m_emissive;
    }

private:
    std::vector<std::shared_ptr<Primitive>> m_primitives;
    std::vector<std::shared_ptr<Primitive>> m_emissive;
    std::vector<std::shared_ptr<Light>>     m_lights;
    std::vector<Primitive*>                 m_unbounded;
    AccelType m_accelType = AccelType::BVH;
    std::unique_ptr<AccelStructure> m_accel;
    bool m_built = false;
};
