// material / bsdf interface. wo and wi point away from the surface (pbrt
// convention); evaluate() returns f(wo, wi) without the cosine factor.

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
    Conductor,
    Emissive,
    Unknown
};

struct MaterialSample {
    glm::dvec3 wi{0.0, 1.0, 0.0};
    // throughput factor for the integrator: f(wo,wi) * |cos(wi,n)| / pdf
    Color weight{0.0, 0.0, 0.0};
    double pdf = 0.0;
    bool valid = false;
    bool delta = false;
    double eta = 1.0;
};

class Material {
public:
    virtual ~Material() = default;
    virtual MaterialType type() const { return MaterialType::Unknown; }

    virtual Color albedo(const HitRecord& rec) const = 0;
    virtual Color emission(const HitRecord&) const { return Color(0.0); }

    virtual Color evaluate(const HitRecord& rec,
                           const glm::dvec3& wo,
                           const glm::dvec3& wi) const = 0;

    // sample one direction. u is in [0,1)^3; the third component is a branch
    // selector for two-lobe materials (reflect vs transmit).
    virtual MaterialSample sample(const HitRecord& rec,
                                  const glm::dvec3& wo,
                                  const glm::dvec3& u) const = 0;

    virtual double pdf(const HitRecord& rec,
                       const glm::dvec3& wo,
                       const glm::dvec3& wi) const = 0;

    virtual bool isDelta() const { return false; }
};

class LambertMaterial : public Material {
public:
    explicit LambertMaterial(const Color& color);
    explicit LambertMaterial(std::shared_ptr<Texture> texture);

    MaterialType type() const override { return MaterialType::Lambert; }
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

private:
    std::shared_ptr<Texture> m_texture;
};

class MirrorMaterial : public Material {
public:
    explicit MirrorMaterial(const Color& reflectance);

    MaterialType type() const override { return MaterialType::Mirror; }
    Color albedo(const HitRecord&) const override { return m_reflectance; }
    Color evaluate(const HitRecord&,
                   const glm::dvec3&,
                   const glm::dvec3&) const override { return Color(0.0); }
    MaterialSample sample(const HitRecord& rec,
                          const glm::dvec3& wo,
                          const glm::dvec3& u) const override;
    double pdf(const HitRecord&,
               const glm::dvec3&,
               const glm::dvec3&) const override { return 0.0; }
    bool isDelta() const override { return true; }

private:
    Color m_reflectance;
};

class DielectricMaterial : public Material {
public:
    DielectricMaterial(double iorOut, double iorIn,
                       const Color& reflectance = Color(1.0),
                       const Color& transmittance = Color(1.0),
                       double roughness = 0.0);

    MaterialType type() const override { return MaterialType::Dielectric; }
    Color albedo(const HitRecord&) const override { return m_reflectance; }
    Color evaluate(const HitRecord& rec,
                   const glm::dvec3& wo,
                   const glm::dvec3& wi) const override;
    MaterialSample sample(const HitRecord& rec,
                          const glm::dvec3& wo,
                          const glm::dvec3& u) const override;
    double pdf(const HitRecord& rec,
               const glm::dvec3& wo,
               const glm::dvec3& wi) const override;
    bool isDelta() const override { return m_roughness <= 1e-4; }

private:
    double m_iorOut;
    double m_iorIn;
    Color  m_reflectance;
    Color  m_transmittance;
    double m_roughness;
    double m_alpha;
};

class ConductorMaterial : public Material {
public:
    ConductorMaterial(const Color& f0, double roughness);

    MaterialType type() const override { return MaterialType::Conductor; }
    Color albedo(const HitRecord&) const override { return m_f0; }
    Color evaluate(const HitRecord& rec,
                   const glm::dvec3& wo,
                   const glm::dvec3& wi) const override;
    MaterialSample sample(const HitRecord& rec,
                          const glm::dvec3& wo,
                          const glm::dvec3& u) const override;
    double pdf(const HitRecord& rec,
               const glm::dvec3& wo,
               const glm::dvec3& wi) const override;
    bool isDelta() const override { return m_roughness <= 1e-4; }

private:
    Color  m_f0;
    double m_roughness;
    double m_alpha;
};

class EmissiveMaterial : public Material {
public:
    explicit EmissiveMaterial(const Color& emission);

    MaterialType type() const override { return MaterialType::Emissive; }
    Color albedo(const HitRecord&) const override { return Color(0.0); }
    Color emission(const HitRecord&) const override { return m_emission; }
    Color evaluate(const HitRecord&,
                   const glm::dvec3&,
                   const glm::dvec3&) const override { return Color(0.0); }
    MaterialSample sample(const HitRecord&,
                          const glm::dvec3&,
                          const glm::dvec3&) const override {
        MaterialSample s;
        s.valid = false;
        return s;
    }
    double pdf(const HitRecord&,
               const glm::dvec3&,
               const glm::dvec3&) const override { return 0.0; }

private:
    Color m_emission;
};
