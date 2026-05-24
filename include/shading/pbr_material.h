// gltf metallic-roughness pbr material with optional normal map and ao.

#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "core/color.h"
#include "shading/material.h"
#include "shading/texture.h"

class ImageTexture;

class PbrMaterial : public Material {
public:
    PbrMaterial();

    // common-case factory: load any subset of (basecolor, roughness, metallic,
    // normal, ao) from `dir`. matches typical ambientCG/polyhaven naming.
    static std::shared_ptr<PbrMaterial> loadFromDirectory(
        const std::string& dir,
        const Color& basecolorFactor = Color(1.0),
        double roughnessFactor = 1.0,
        double metallicFactor = 1.0);

    void setBasecolor(std::shared_ptr<Texture> t)        { m_basecolor = std::move(t); }
    void setRoughness(std::shared_ptr<ImageTexture> t)   { m_roughness = std::move(t); }
    void setMetallic(std::shared_ptr<ImageTexture> t)    { m_metallic  = std::move(t); }
    void setNormal(std::shared_ptr<ImageTexture> t)      { m_normal    = std::move(t); }
    void setAO(std::shared_ptr<ImageTexture> t)          { m_ao        = std::move(t); }

    void setBasecolorFactor(const Color& c) { m_basecolorFactor = c; }
    void setRoughnessFactor(double r)       { m_roughnessFactor = r; }
    void setMetallicFactor(double m)        { m_metallicFactor  = m; }

    MaterialType type() const override { return MaterialType::Unknown; }
    Color albedo(const HitRecord& rec) const override;
    Color evaluate(const HitRecord& rec,
                   const glm::dvec3& wo,
                   const glm::dvec3& wi) const override;
    MaterialSample sample(const HitRecord& rec,
                          const glm::dvec3& wo,
                          const glm::dvec3& u) const override;
    double pdf(const HitRecord& rec,
               const glm::dvec3& wo,
               const glm::dvec3& wi) const override;
    bool isDelta() const override { return false; }

private:
    struct Params {
        Color basecolor;
        double roughness;
        double metallic;
        double ao;
        glm::dvec3 shadingNormal;
        glm::dvec3 tangent;
    };
    Params sampleParams(const HitRecord& rec) const;

    std::shared_ptr<Texture>      m_basecolor;       // sRGB
    std::shared_ptr<ImageTexture> m_roughness;       // linear (red channel)
    std::shared_ptr<ImageTexture> m_metallic;        // linear (red channel; or blue for orm)
    std::shared_ptr<ImageTexture> m_normal;          // linear tangent-space
    std::shared_ptr<ImageTexture> m_ao;              // linear scalar

    Color  m_basecolorFactor{1.0};
    double m_roughnessFactor = 1.0;
    double m_metallicFactor  = 1.0;
};
