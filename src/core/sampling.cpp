// sampling primitives implementation.

#include "core/sampling.h"

#include <algorithm>
#include <cmath>

#include "core/constants.h"

namespace sampling {

glm::dvec2 concentricDisk(double u1, double u2) {
    double sx = 2.0 * u1 - 1.0;
    double sy = 2.0 * u2 - 1.0;
    if (sx == 0.0 && sy == 0.0) return glm::dvec2(0.0);
    double r, theta;
    if (std::abs(sx) > std::abs(sy)) {
        r = sx;
        theta = (constants::kPi / 4.0) * (sy / sx);
    } else {
        r = sy;
        theta = (constants::kPi / 2.0) - (constants::kPi / 4.0) * (sx / sy);
    }
    return glm::dvec2(r * std::cos(theta), r * std::sin(theta));
}

glm::dvec3 cosineHemisphere(double u1, double u2) {
    glm::dvec2 d = concentricDisk(u1, u2);
    double z = std::sqrt(std::max(0.0, 1.0 - d.x * d.x - d.y * d.y));
    return glm::dvec3(d.x, d.y, z);
}

double cosineHemispherePdf(double cosTheta) {
    return std::max(0.0, cosTheta) / constants::kPi;
}

glm::dvec3 uniformSphere(double u1, double u2) {
    double z = 1.0 - 2.0 * u1;
    double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    double phi = 2.0 * constants::kPi * u2;
    return glm::dvec3(r * std::cos(phi), r * std::sin(phi), z);
}

double uniformSpherePdf() { return 1.0 / (4.0 * constants::kPi); }

glm::dvec3 uniformHemisphere(double u1, double u2) {
    double z = u1;
    double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    double phi = 2.0 * constants::kPi * u2;
    return glm::dvec3(r * std::cos(phi), r * std::sin(phi), z);
}

double uniformHemispherePdf() { return 1.0 / (2.0 * constants::kPi); }

// heitz 2018 sampling of the visible normal distribution
glm::dvec3 ggxVndf(const glm::dvec3& wo, double ax, double ay,
                   double u1, double u2) {
    glm::dvec3 vh = glm::normalize(glm::dvec3(ax * wo.x, ay * wo.y, wo.z));
    double lensq = vh.x * vh.x + vh.y * vh.y;
    glm::dvec3 t1 = lensq > 0.0
        ? glm::dvec3(-vh.y, vh.x, 0.0) / std::sqrt(lensq)
        : glm::dvec3(1.0, 0.0, 0.0);
    glm::dvec3 t2 = glm::cross(vh, t1);
    double r = std::sqrt(u1);
    double phi = 2.0 * constants::kPi * u2;
    double p1 = r * std::cos(phi);
    double p2 = r * std::sin(phi);
    double s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * std::sqrt(1.0 - p1 * p1) + s * p2;
    glm::dvec3 nh = p1 * t1 + p2 * t2 +
                    std::sqrt(std::max(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return glm::normalize(glm::dvec3(ax * nh.x, ay * nh.y, std::max(0.0, nh.z)));
}

double ggxD(const glm::dvec3& wh, double ax, double ay) {
    double cos2 = wh.z * wh.z;
    if (cos2 <= 0.0) return 0.0;
    double sin2 = std::max(0.0, 1.0 - cos2);
    if (sin2 == 0.0) {
        double t = 1.0 / (ax * ay);
        return t * t / constants::kPi;
    }
    double cosPhi2 = (sin2 == 0.0) ? 1.0 : (wh.x * wh.x) / sin2;
    double sinPhi2 = (sin2 == 0.0) ? 0.0 : (wh.y * wh.y) / sin2;
    double tan2 = sin2 / cos2;
    double a2 = cosPhi2 / (ax * ax) + sinPhi2 / (ay * ay);
    double e = 1.0 + tan2 * a2;
    return 1.0 / (constants::kPi * ax * ay * cos2 * cos2 * e * e);
}

double ggxLambda(const glm::dvec3& w, double ax, double ay) {
    double cos2 = w.z * w.z;
    if (cos2 <= 0.0) return 0.0;
    double sin2 = std::max(0.0, 1.0 - cos2);
    if (sin2 == 0.0) return 0.0;
    double cosPhi2 = (w.x * w.x) / sin2;
    double sinPhi2 = (w.y * w.y) / sin2;
    double alpha2 = cosPhi2 * ax * ax + sinPhi2 * ay * ay;
    double tan2 = sin2 / cos2;
    return 0.5 * (-1.0 + std::sqrt(1.0 + alpha2 * tan2));
}

double ggxG1(const glm::dvec3& w, double ax, double ay) {
    return 1.0 / (1.0 + ggxLambda(w, ax, ay));
}

double ggxG(const glm::dvec3& wo, const glm::dvec3& wi, double ax, double ay) {
    return 1.0 / (1.0 + ggxLambda(wo, ax, ay) + ggxLambda(wi, ax, ay));
}

double ggxVndfPdfReflect(const glm::dvec3& wo, const glm::dvec3& wh,
                         double ax, double ay) {
    double cosThetaO = std::abs(wo.z);
    if (cosThetaO == 0.0) return 0.0;
    double D = ggxD(wh, ax, ay);
    double G1o = ggxG1(wo, ax, ay);
    double dwh_dwi = 1.0 / (4.0 * std::abs(glm::dot(wo, wh)));
    return D * G1o * std::abs(glm::dot(wo, wh)) / cosThetaO * dwh_dwi;
}

double ggxVndfPdfRefract(const glm::dvec3& wo, const glm::dvec3& wi,
                         const glm::dvec3& wh,
                         double eta, double ax, double ay) {
    double cosThetaO = std::abs(wo.z);
    if (cosThetaO == 0.0) return 0.0;
    double D = ggxD(wh, ax, ay);
    double G1o = ggxG1(wo, ax, ay);
    double denom = glm::dot(wi, wh) * eta + glm::dot(wo, wh);
    if (denom == 0.0) return 0.0;
    double dwh_dwi = std::abs(glm::dot(wi, wh)) / (denom * denom);
    return D * G1o * std::abs(glm::dot(wo, wh)) / cosThetaO * dwh_dwi;
}

} // namespace sampling
