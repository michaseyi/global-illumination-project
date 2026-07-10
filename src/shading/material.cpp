#include "shading/material.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "core/constants.h"
#include "core/frame.h"
#include "core/sampling.h"

namespace {

double fresnelDielectric(double cosThetaI, double etaI, double etaT) {
    cosThetaI = std::max(-1.0, std::min(1.0, cosThetaI));
    bool entering = cosThetaI > 0.0;
    if (!entering) {
        std::swap(etaI, etaT);
        cosThetaI = std::abs(cosThetaI);
    }
    double sinThetaI = std::sqrt(std::max(0.0, 1.0 - cosThetaI * cosThetaI));
    double sinThetaT = etaI / etaT * sinThetaI;
    if (sinThetaT >= 1.0) return 1.0;
    double cosThetaT = std::sqrt(std::max(0.0, 1.0 - sinThetaT * sinThetaT));
    double rPar  = ((etaT * cosThetaI) - (etaI * cosThetaT)) /
                   ((etaT * cosThetaI) + (etaI * cosThetaT));
    double rPerp = ((etaI * cosThetaI) - (etaT * cosThetaT)) /
                   ((etaI * cosThetaI) + (etaT * cosThetaT));
    return 0.5 * (rPar * rPar + rPerp * rPerp);
}

Color schlick(const Color& f0, double cosTheta) {
    double k = std::pow(std::max(0.0, 1.0 - cosTheta), 5.0);
    return f0 + (Color(1.0) - f0) * k;
}

// reflect/refract in tangent space (normal = +z)
glm::dvec3 reflectLocal(const glm::dvec3& wo) {
    return glm::dvec3(-wo.x, -wo.y, wo.z);
}

bool refractLocal(const glm::dvec3& wi, double eta, glm::dvec3& wt) {
    // wi points away from the surface in tangent space; n = (0,0,1) on its side.
    double cosThetaI = wi.z;
    glm::dvec3 n(0.0, 0.0, 1.0);
    if (cosThetaI < 0.0) {
        n = -n;
        cosThetaI = -cosThetaI;
    }
    double sin2I = std::max(0.0, 1.0 - cosThetaI * cosThetaI);
    double sin2T = eta * eta * sin2I;
    if (sin2T >= 1.0) return false;
    double cosThetaT = std::sqrt(1.0 - sin2T);
    wt = -eta * wi + (eta * cosThetaI - cosThetaT) * n;
    return true;
}

double roughnessToAlpha(double r) {
    return std::max(1e-4, r * r);
}

} // namespace

// Lambert
LambertMaterial::LambertMaterial(const Color& color)
    : m_texture(std::make_shared<ConstantTexture>(color)) {}

LambertMaterial::LambertMaterial(std::shared_ptr<Texture> texture)
    : m_texture(std::move(texture)) {}

Color LambertMaterial::albedo(const HitRecord& rec) const {
    return m_texture->value(rec.uv, rec.position);
}

Color LambertMaterial::evaluate(const HitRecord& rec,
                                const glm::dvec3& wo,
                                const glm::dvec3& wi) const {
    if (glm::dot(rec.shadingNormal, wi) <= 0.0) return Color(0.0);
    if (glm::dot(rec.shadingNormal, wo) <= 0.0) return Color(0.0);
    return albedo(rec) / constants::kPi;
}

MaterialSample LambertMaterial::sample(const HitRecord& rec,
                                       const glm::dvec3& wo,
                                       const glm::dvec3& u) const {
    MaterialSample s;
    if (glm::dot(rec.shadingNormal, wo) <= 0.0) return s;
    Frame f(rec.shadingNormal);
    glm::dvec3 wiLocal = sampling::cosineHemisphere(u.x, u.y);
    glm::dvec3 wi = f.toWorld(wiLocal);
    s.wi = wi;
    s.pdf = sampling::cosineHemispherePdf(wiLocal.z);
    s.weight = albedo(rec); // f*cos/pdf = (rho/pi) * cos / (cos/pi) = rho
    s.valid = s.pdf > 0.0;
    return s;
}

double LambertMaterial::pdf(const HitRecord& rec,
                            const glm::dvec3& wo,
                            const glm::dvec3& wi) const {
    if (glm::dot(rec.shadingNormal, wi) <= 0.0) return 0.0;
    if (glm::dot(rec.shadingNormal, wo) <= 0.0) return 0.0;
    return sampling::cosineHemispherePdf(glm::dot(rec.shadingNormal, wi));
}

// Mirror
MirrorMaterial::MirrorMaterial(const Color& reflectance)
    : m_reflectance(reflectance) {}

MaterialSample MirrorMaterial::sample(const HitRecord& rec,
                                      const glm::dvec3& wo,
                                      const glm::dvec3& /*u*/) const {
    MaterialSample s;
    glm::dvec3 n = rec.shadingNormal;
    if (glm::dot(wo, n) <= 0.0) return s;
    s.wi = glm::reflect(-wo, n);
    s.weight = m_reflectance;
    s.pdf = 1.0;
    s.delta = true;
    s.valid = true;
    return s;
}

// Dielectric

DielectricMaterial::DielectricMaterial(double iorOut, double iorIn,
                                       const Color& reflectance,
                                       const Color& transmittance,
                                       double roughness,
                                       bool thin)
    : m_iorOut(iorOut),
      m_iorIn(iorIn),
      m_reflectance(reflectance),
      m_transmittance(transmittance),
      m_roughness(roughness),
      m_alpha(roughnessToAlpha(roughness)),
      m_thin(thin) {}

Color DielectricMaterial::evaluate(const HitRecord& rec,
                                   const glm::dvec3& wo,
                                   const glm::dvec3& wi) const {
    if (isDelta()) return Color(0.0);

    Frame f(rec.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    glm::dvec3 wiL = f.toLocal(wi);
    bool reflect = woL.z * wiL.z > 0.0;

    double etaI = woL.z > 0.0 ? m_iorOut : m_iorIn;
    double etaT = woL.z > 0.0 ? m_iorIn  : m_iorOut;

    if (reflect) {
        glm::dvec3 wh = glm::normalize(woL + wiL);
        if (wh.z < 0.0) wh = -wh;
        double D  = sampling::ggxD(wh, m_alpha, m_alpha);
        double G  = sampling::ggxG(woL, wiL, m_alpha, m_alpha);
        double F  = fresnelDielectric(glm::dot(woL, wh), etaI, etaT);
        double denom = 4.0 * std::abs(woL.z) * std::abs(wiL.z);
        if (denom <= 0.0) return Color(0.0);
        return m_reflectance * (F * D * G / denom);
    } else {
        double eta = etaT / etaI;
        glm::dvec3 wh = -glm::normalize(woL + wiL * (etaT / etaI));
        // robust form
        wh = glm::normalize(-(etaI * woL + etaT * wiL));
        if (wh.z < 0.0) wh = -wh;
        double sqrtDenom = glm::dot(woL, wh) + (etaT / etaI) * glm::dot(wiL, wh);
        if (std::abs(sqrtDenom) < 1e-12) return Color(0.0);
        double D = sampling::ggxD(wh, m_alpha, m_alpha);
        double G = sampling::ggxG(woL, wiL, m_alpha, m_alpha);
        double F = fresnelDielectric(glm::dot(woL, wh), etaI, etaT);
        double factor = std::abs(glm::dot(wiL, wh)) * std::abs(glm::dot(woL, wh)) /
                        (std::abs(woL.z) * std::abs(wiL.z));
        double t = (1.0 - F) * D * G * (etaT * etaT) / (sqrtDenom * sqrtDenom);
        return m_transmittance * (factor * t);
    }
}

MaterialSample DielectricMaterial::sample(const HitRecord& rec,
                                          const glm::dvec3& wo,
                                          const glm::dvec3& u) const {
    Frame f(rec.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    if (woL.z == 0.0) return MaterialSample{};

    if (m_thin) {
        // thin sheet: the pane's two parallel faces cancel refraction, so
        // transmission continues straight through. reflectance accounts for
        // both interfaces (series sum 2F/(1+F)).
        double F = fresnelDielectric(std::abs(woL.z), m_iorOut, m_iorIn);
        F = 2.0 * F / (1.0 + F);
        MaterialSample s;
        if (u.z < F) {
            glm::dvec3 wiL(-woL.x, -woL.y, woL.z);  // mirror about the pane
            s.wi = f.toWorld(wiL);
            s.weight = m_reflectance;
            s.pdf = F;
        } else {
            s.wi = -wo;  // straight through, tinted
            s.weight = m_transmittance;
            s.pdf = 1.0 - F;
        }
        s.delta = true;
        s.valid = true;
        return s;
    }

    bool entering = woL.z > 0.0;
    double etaI = entering ? m_iorOut : m_iorIn;
    double etaT = entering ? m_iorIn  : m_iorOut;
    double etaRatio = etaI / etaT; // for refractLocal

    // pick a half-vector. smooth case => wh is the surface normal.
    glm::dvec3 wh;
    if (isDelta()) {
        wh = glm::dvec3(0.0, 0.0, woL.z > 0.0 ? 1.0 : -1.0);
    } else {
        glm::dvec3 woFlip = woL.z >= 0.0 ? woL : -woL;
        wh = sampling::ggxVndf(woFlip, m_alpha, m_alpha, u.x, u.y);
        if (woL.z < 0.0) wh = -wh;
    }

    double cosWoWh = glm::dot(woL, wh);
    double F = fresnelDielectric(cosWoWh, etaI, etaT);

    MaterialSample s;
    if (u.z < F) {
        // reflection branch
        glm::dvec3 wiL = -woL + 2.0 * glm::dot(woL, wh) * wh;
        if (wiL.z * woL.z <= 0.0) return s;
        s.wi = f.toWorld(wiL);

        if (isDelta()) {
            s.weight = m_reflectance;
            s.pdf = F;
            s.delta = true;
            s.valid = true;
        } else {
            double D = sampling::ggxD(wh, m_alpha, m_alpha);
            double G = sampling::ggxG(woL, wiL, m_alpha, m_alpha);
            double brdf = D * G * F / (4.0 * std::abs(woL.z) * std::abs(wiL.z));
            double pdfH = sampling::ggxVndfPdfReflect(woL, wh, m_alpha, m_alpha);
            s.pdf = F * pdfH;
            if (s.pdf <= 0.0) return MaterialSample{};
            s.weight = m_reflectance * (brdf * std::abs(wiL.z) / s.pdf);
            s.valid = true;
        }
    } else {
        // refraction branch
        glm::dvec3 wiL;
        if (!refractLocal(woL, etaRatio, wiL)) {
            // total internal reflection — fall back to reflection
            glm::dvec3 refl = -woL + 2.0 * glm::dot(woL, wh) * wh;
            if (refl.z * woL.z <= 0.0) return s;
            s.wi = f.toWorld(refl);
            s.weight = m_reflectance;
            s.pdf = 1.0;
            s.delta = isDelta();
            s.valid = true;
            return s;
        }

        s.wi = f.toWorld(wiL);
        s.eta = etaRatio;

        if (isDelta()) {
            // power compression by eta^2 cancels with measure-conversion
            double factor = (etaI * etaI) / (etaT * etaT);
            s.weight = m_transmittance * factor;
            s.pdf = 1.0 - F;
            s.delta = true;
            s.valid = true;
        } else {
            double D = sampling::ggxD(wh, m_alpha, m_alpha);
            double G = sampling::ggxG(woL, wiL, m_alpha, m_alpha);
            double sqrtDenom = etaI * glm::dot(woL, wh) + etaT * glm::dot(wiL, wh);
            if (std::abs(sqrtDenom) < 1e-12) return MaterialSample{};
            double btdf = std::abs((1.0 - F) * D * G * etaT * etaT *
                                   std::abs(glm::dot(woL, wh)) *
                                   std::abs(glm::dot(wiL, wh)) /
                                   (std::abs(woL.z) * std::abs(wiL.z) *
                                    sqrtDenom * sqrtDenom));
            double pdfH = sampling::ggxVndfPdfRefract(woL, wiL, wh,
                                                      etaT / etaI,
                                                      m_alpha, m_alpha);
            s.pdf = (1.0 - F) * pdfH;
            if (s.pdf <= 0.0) return MaterialSample{};
            s.weight = m_transmittance * (btdf * std::abs(wiL.z) / s.pdf);
            s.valid = true;
        }
    }
    return s;
}

double DielectricMaterial::pdf(const HitRecord& rec,
                               const glm::dvec3& wo,
                               const glm::dvec3& wi) const {
    if (isDelta()) return 0.0;
    Frame f(rec.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    glm::dvec3 wiL = f.toLocal(wi);
    bool reflect = woL.z * wiL.z > 0.0;
    bool entering = woL.z > 0.0;
    double etaI = entering ? m_iorOut : m_iorIn;
    double etaT = entering ? m_iorIn  : m_iorOut;
    glm::dvec3 wh;
    double F;
    if (reflect) {
        wh = glm::normalize(woL + wiL);
        if (wh.z < 0.0) wh = -wh;
        F = fresnelDielectric(glm::dot(woL, wh), etaI, etaT);
        double pdfH = sampling::ggxVndfPdfReflect(woL, wh, m_alpha, m_alpha);
        return F * pdfH;
    } else {
        wh = -glm::normalize(etaI * woL + etaT * wiL);
        if (wh.z < 0.0) wh = -wh;
        F = fresnelDielectric(glm::dot(woL, wh), etaI, etaT);
        double pdfH = sampling::ggxVndfPdfRefract(woL, wiL, wh,
                                                  etaT / etaI,
                                                  m_alpha, m_alpha);
        return (1.0 - F) * pdfH;
    }
}

// Conductor
ConductorMaterial::ConductorMaterial(const Color& f0, double roughness)
    : m_f0(f0), m_roughness(roughness), m_alpha(roughnessToAlpha(roughness)) {}

Color ConductorMaterial::evaluate(const HitRecord& rec,
                                  const glm::dvec3& wo,
                                  const glm::dvec3& wi) const {
    if (isDelta()) return Color(0.0);
    Frame f(rec.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    glm::dvec3 wiL = f.toLocal(wi);
    if (woL.z <= 0.0 || wiL.z <= 0.0) return Color(0.0);
    glm::dvec3 wh = glm::normalize(woL + wiL);
    double D = sampling::ggxD(wh, m_alpha, m_alpha);
    double G = sampling::ggxG(woL, wiL, m_alpha, m_alpha);
    Color F = schlick(m_f0, std::max(0.0, glm::dot(woL, wh)));
    double denom = 4.0 * woL.z * wiL.z;
    if (denom <= 0.0) return Color(0.0);
    return F * (D * G / denom);
}

MaterialSample ConductorMaterial::sample(const HitRecord& rec,
                                         const glm::dvec3& wo,
                                         const glm::dvec3& u) const {
    MaterialSample s;
    Frame f(rec.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    if (woL.z <= 0.0) return s;

    if (isDelta()) {
        glm::dvec3 wiL(-woL.x, -woL.y, woL.z);
        s.wi = f.toWorld(wiL);
        Color F = schlick(m_f0, woL.z);
        s.weight = F;
        s.pdf = 1.0;
        s.delta = true;
        s.valid = true;
        return s;
    }

    glm::dvec3 wh = sampling::ggxVndf(woL, m_alpha, m_alpha, u.x, u.y);
    glm::dvec3 wiL = -woL + 2.0 * glm::dot(woL, wh) * wh;
    if (wiL.z <= 0.0) return s;
    s.wi = f.toWorld(wiL);

    double D = sampling::ggxD(wh, m_alpha, m_alpha);
    double G = sampling::ggxG(woL, wiL, m_alpha, m_alpha);
    Color F = schlick(m_f0, std::max(0.0, glm::dot(woL, wh)));
    double brdfScalar = D * G / (4.0 * woL.z * wiL.z);
    double pdfH = sampling::ggxVndfPdfReflect(woL, wh, m_alpha, m_alpha);
    if (pdfH <= 0.0) return MaterialSample{};
    s.pdf = pdfH;
    s.weight = F * (brdfScalar * wiL.z / pdfH);
    s.valid = true;
    return s;
}

double ConductorMaterial::pdf(const HitRecord& rec,
                              const glm::dvec3& wo,
                              const glm::dvec3& wi) const {
    if (isDelta()) return 0.0;
    Frame f(rec.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    glm::dvec3 wiL = f.toLocal(wi);
    if (woL.z <= 0.0 || wiL.z <= 0.0) return 0.0;
    glm::dvec3 wh = glm::normalize(woL + wiL);
    return sampling::ggxVndfPdfReflect(woL, wh, m_alpha, m_alpha);
}

// Emissive
EmissiveMaterial::EmissiveMaterial(const Color& emission)
    : m_emission(emission) {}
