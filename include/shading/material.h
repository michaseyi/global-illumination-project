// Material describes how a surface scatters and emits light.

#pragma once

#include <memory>
#include <glm/glm.hpp>

#include "core/color.h"
#include "scene/hit_record.h"
#include "shading/texture.h"

enum class MaterialType {
    Lambert,
    Mirror,
    Dielectric,
    Emissive,
    Unknown
};

struct MaterialSample {
    // Sampled incoming direction in world space.
    glm::dvec3 wi{0.0, 1.0, 0.0};

    // Throughput multiplier carried by this sample.
    Color weight{0.0, 0.0, 0.0};

    // Probability density for the sampled direction.
    double pdf = 0.0;

    bool valid = false;

    // True for perfect mirror-like events.
    bool delta = false;
};

class Material {
public:
    virtual ~Material() = default;

    // Returns the high-level material category.
    virtual MaterialType type() const {
        return MaterialType::Unknown;
    }

    // Base surface color used by simple shading models.
    virtual Color albedo(const HitRecord& rec) const = 0;

    // Self-emission. Default is black.
    virtual Color emission(const HitRecord&) const {
        return Color(0.0, 0.0, 0.0);
    }

    // Evaluate the scattering function f(wo, wi).
    virtual Color evaluate(const HitRecord& rec,
                           const glm::dvec3& wo,
                           const glm::dvec3& wi) const = 0;

    // Sample one incoming direction. This matters once stochastic integrators are added.
    virtual MaterialSample sample(const HitRecord& rec,
                                  const glm::dvec3& wo) const;

    // PDF associated with sample().
    virtual double pdf(const HitRecord& rec,
                       const glm::dvec3& wo,
                       const glm::dvec3& wi) const;

    // Whether the material behaves as a delta distribution.
    virtual bool isDelta() const {
        return false;
    }
};

class LambertMaterial : public Material {
public:
    explicit LambertMaterial(const Color& color);
    explicit LambertMaterial(std::shared_ptr<Texture> texture);

    MaterialType type() const override {
        return MaterialType::Lambert;
    }

    Color albedo(const HitRecord& rec) const override;
    Color evaluate(const HitRecord& rec,
                   const glm::dvec3& wo,
                   const glm::dvec3& wi) const override;

private:
    std::shared_ptr<Texture> m_texture;
};
