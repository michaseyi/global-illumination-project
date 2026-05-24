// light sources visible to the integrator.

#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "core/color.h"

class Primitive;
class HitRecord;
struct ShapeSample;

struct LightSample {
    glm::dvec3 wi{0.0, 1.0, 0.0};        // direction from reference toward light
    glm::dvec3 position{0.0};             // sampled point on the light
    glm::dvec3 normal{0.0, 1.0, 0.0};    // normal at the sampled point
    Color L{0.0};                         // emitted radiance toward the reference
    double pdf = 0.0;                     // wrt solid angle at the reference
    double distance = 0.0;                // |position - ref|
    bool isDelta = false;
};

class Light {
public:
    virtual ~Light() = default;

    // sample a direction toward the light from `ref` (and normal).
    virtual LightSample sampleLi(const glm::dvec3& ref,
                                 const glm::dvec3& refNormal,
                                 double u1, double u2) const = 0;

    // pdf in solid-angle measure of sampling direction `wi` toward this light
    // from reference `ref`. used by mis.
    virtual double pdfLi(const glm::dvec3& ref,
                         const glm::dvec3& wi) const { (void)ref; (void)wi; return 0.0; }

    virtual bool isDelta() const = 0;
};

// classic point light (singular, delta).
class PointLight : public Light {
public:
    PointLight(const glm::dvec3& position,
               const Color& intensityColor,
               double intensity);

    const glm::dvec3& position() const;
    Color intensity() const;

    LightSample sampleLi(const glm::dvec3& ref,
                         const glm::dvec3& refNormal,
                         double u1, double u2) const override;
    bool isDelta() const override { return true; }

private:
    glm::dvec3 m_position;
    Color m_intensityColor;
    double m_intensity;
};

// diffuse area light bound to a primitive. emits constant radiance Le on its
// front side (the side opposite the outward normal flip).
class AreaLight : public Light {
public:
    AreaLight(std::shared_ptr<Primitive> shape, const Color& emission,
              bool twoSided = false);

    LightSample sampleLi(const glm::dvec3& ref,
                         const glm::dvec3& refNormal,
                         double u1, double u2) const override;
    double pdfLi(const glm::dvec3& ref,
                 const glm::dvec3& wi) const override;
    bool isDelta() const override { return false; }

    // emitted radiance from this light along `w` at a surface point with normal n.
    Color L(const glm::dvec3& n, const glm::dvec3& w) const;

    const Color& emission() const { return m_emission; }
    Primitive* shape() const { return m_shape.get(); }
    bool twoSided() const { return m_twoSided; }

private:
    std::shared_ptr<Primitive> m_shape;
    Color m_emission;
    bool  m_twoSided;
};
