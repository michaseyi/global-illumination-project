// monte carlo sampling primitives in pbrt style.

#pragma once

#include <glm/glm.hpp>

namespace sampling {

glm::dvec2 concentricDisk(double u1, double u2);
glm::dvec3 cosineHemisphere(double u1, double u2);
double     cosineHemispherePdf(double cosTheta);

glm::dvec3 uniformSphere(double u1, double u2);
double     uniformSpherePdf();

glm::dvec3 uniformHemisphere(double u1, double u2);
double     uniformHemispherePdf();

// trowbridge-reitz visible normal distribution sampling.
// alpha is roughness^2 (anisotropic ax/ay supported).
glm::dvec3 ggxVndf(const glm::dvec3& wo, double ax, double ay,
                   double u1, double u2);

// trowbridge-reitz D and Lambda terms (anisotropic).
double ggxD(const glm::dvec3& wh, double ax, double ay);
double ggxLambda(const glm::dvec3& w, double ax, double ay);
double ggxG1(const glm::dvec3& w, double ax, double ay);
double ggxG(const glm::dvec3& wo, const glm::dvec3& wi, double ax, double ay);

// pdf of the ggx vndf sample expressed for the reflected direction wi
double ggxVndfPdfReflect(const glm::dvec3& wo, const glm::dvec3& wh,
                         double ax, double ay);

// pdf for transmitted direction in dielectric ggx
double ggxVndfPdfRefract(const glm::dvec3& wo, const glm::dvec3& wi,
                         const glm::dvec3& wh,
                         double eta, double ax, double ay);

inline double powerHeuristic(double pdfA, double pdfB) {
    double a = pdfA * pdfA;
    double b = pdfB * pdfB;
    double s = a + b;
    return s > 0.0 ? a / s : 0.0;
}

} // namespace sampling
