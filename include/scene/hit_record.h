// HitRecord is produced by geometry and consumed by shading/integrators.

#pragma once

#include <glm/glm.hpp>
#include <memory>

class Material;

struct HitRecord {
    // Ray parameter at the hit point.
    double t = 0.0;

    // World-space hit position.
    glm::dvec3 position{0.0, 0.0, 0.0};

    // True geometric normal from the shape itself.
    glm::dvec3 geometricNormal{0.0, 1.0, 0.0};

    // Normal used for shading. It may differ later for smooth shading or normal mapping.
    glm::dvec3 shadingNormal{0.0, 1.0, 0.0};

    // Surface coordinates for textures.
    glm::dvec2 uv{0.0, 0.0};

    // True when the ray hits the front side of the surface.
    bool frontFace = true;

    // Material attached to the hit primitive.
    std::shared_ptr<Material> material;

    // Flip the stored normal so it always points against the incoming ray.
    void setFaceNormal(const glm::dvec3& rayDirection,
                       const glm::dvec3& outwardNormal) {
        frontFace = glm::dot(rayDirection, outwardNormal) < 0.0;
        geometricNormal = frontFace ? outwardNormal : -outwardNormal;
        shadingNormal   = geometricNormal;
    }
};
