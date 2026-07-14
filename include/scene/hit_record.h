// HitRecord is produced by geometry and consumed by shading/integrators.

#pragma once

#include <glm/glm.hpp>
#include <memory>

class Material;
class AreaLight;
class Primitive;

struct HitRecord {
    double t = 0.0;

    glm::dvec3 position{0.0, 0.0, 0.0};
    glm::dvec3 geometricNormal{0.0, 1.0, 0.0};
    glm::dvec3 shadingNormal{0.0, 1.0, 0.0};
    glm::dvec2 uv{0.0, 0.0};

    bool frontFace = true;

    // pbrt's dpdu in world space (used for normal mapping when present).
    glm::dvec3 tangent{1.0, 0.0, 0.0};
    bool hasTangent = false;

    std::shared_ptr<Material> material;

    AreaLight* areaLight = nullptr;
    const Primitive* primitive = nullptr;

    void setFaceNormal(const glm::dvec3& rayDirection,
                       const glm::dvec3& outwardNormal) {
        frontFace = glm::dot(rayDirection, outwardNormal) < 0.0;
        geometricNormal = frontFace ? outwardNormal : -outwardNormal;
        shadingNormal   = geometricNormal;
    }
};
