// light sources visible to the integrator.

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

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

// distant directional light (a "sun"): parallel rays, constant irradiance
// regardless of position. delta light.
class DirectionalLight : public Light {
public:
    // `direction` is the travel direction of the light (from sun to scene).
    DirectionalLight(const glm::dvec3& direction, const Color& irradiance);

    LightSample sampleLi(const glm::dvec3& ref,
                         const glm::dvec3& refNormal,
                         double u1, double u2) const override;
    bool isDelta() const override { return true; }

private:
    glm::dvec3 m_direction;   // normalized travel direction
    Color m_irradiance;
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
    virtual Color L(const glm::dvec3& n, const glm::dvec3& w) const;

    // total emitting area; used by the integrator to convert the light-sample
    // pdf between area and solid-angle measure (mesh lights override it).
    virtual double totalArea() const;

    const Color& emission() const { return m_emission; }
    Primitive* shape() const { return m_shape.get(); }
    bool twoSided() const { return m_twoSided; }

protected:
    AreaLight(const Color& emission, bool twoSided)
        : m_shape(nullptr), m_emission(emission), m_twoSided(twoSided) {}

    std::shared_ptr<Primitive> m_shape;
    Color m_emission;
    bool  m_twoSided;
};

// area light over a group of triangles (an emissive mesh). samples a point on
// the whole set proportional to triangle area, so a finely tessellated fixture
// is ONE light instead of thousands - this is what makes next-event estimation
// tractable for emissive meshes (uniform per-triangle lights give fireflies).
class MeshAreaLight : public AreaLight {
public:
    MeshAreaLight(std::vector<Primitive*> triangles,
                  const Color& emission, bool twoSided = true);

    LightSample sampleLi(const glm::dvec3& ref,
                         const glm::dvec3& refNormal,
                         double u1, double u2) const override;
    double totalArea() const override { return m_totalArea; }

private:
    std::vector<Primitive*> m_tris;
    std::vector<double> m_cdf;   // cumulative area; m_cdf.back() == m_totalArea
    double m_totalArea = 0.0;
};
