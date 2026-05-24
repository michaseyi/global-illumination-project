#include "shading/pbr_material.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sys/stat.h>
#include <dirent.h>

#include <glm/glm.hpp>

#include "core/constants.h"
#include "core/frame.h"
#include "core/sampling.h"
#include "shading/image_texture.h"

namespace {

double luminance(const Color& c) {
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

Color schlickF(const Color& f0, double cosTheta) {
    double k = std::pow(std::max(0.0, 1.0 - cosTheta), 5.0);
    return f0 + (Color(1.0) - f0) * k;
}

double clampPickProb(double p) { return std::max(0.05, std::min(0.95, p)); }

bool nameMatches(const std::string& fname, std::initializer_list<const char*> needles) {
    std::string lower = fname;
    for (auto& c : lower) c = char(std::tolower(c));
    for (auto* n : needles) {
        if (lower.find(n) != std::string::npos) return true;
    }
    return false;
}

bool isImage(const std::string& fname) {
    return nameMatches(fname, {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr"});
}

} // namespace

PbrMaterial::PbrMaterial() = default;

Color PbrMaterial::albedo(const HitRecord& rec) const {
    Params p = sampleParams(rec);
    return p.basecolor;
}

PbrMaterial::Params PbrMaterial::sampleParams(const HitRecord& rec) const {
    Params p;
    Color bc = m_basecolor
        ? m_basecolor->value(rec.uv, rec.position)
        : Color(1.0);
    p.basecolor = bc * m_basecolorFactor;

    p.roughness = m_roughness
        ? std::max(0.04, m_roughness->sampleRGBA(rec.uv).r) * m_roughnessFactor
        : m_roughnessFactor;
    p.roughness = std::max(0.04, std::min(1.0, p.roughness));

    p.metallic = m_metallic
        ? m_metallic->sampleRGBA(rec.uv).r * m_metallicFactor
        : m_metallicFactor;
    p.metallic = std::max(0.0, std::min(1.0, p.metallic));

    p.ao = m_ao ? m_ao->sampleRGBA(rec.uv).r : 1.0;

    p.shadingNormal = rec.shadingNormal;
    p.tangent       = rec.tangent;

    if (m_normal && rec.hasTangent) {
        glm::dvec4 n = m_normal->sampleRGBA(rec.uv);
        glm::dvec3 nt(2.0 * n.r - 1.0, 2.0 * n.g - 1.0, 2.0 * n.b - 1.0);
        if (glm::length(nt) > 1e-6) {
            nt = glm::normalize(nt);
            glm::dvec3 N = rec.shadingNormal;
            glm::dvec3 T = glm::normalize(rec.tangent - N * glm::dot(N, rec.tangent));
            glm::dvec3 B = glm::cross(N, T);
            p.shadingNormal = glm::normalize(nt.x * T + nt.y * B + nt.z * N);
            p.tangent = T;
        }
    }
    return p;
}

Color PbrMaterial::evaluate(const HitRecord& rec,
                            const glm::dvec3& wo,
                            const glm::dvec3& wi) const {
    Params p = sampleParams(rec);
    Frame f(p.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    glm::dvec3 wiL = f.toLocal(wi);
    if (woL.z <= 0.0 || wiL.z <= 0.0) return Color(0.0);

    glm::dvec3 wh = glm::normalize(woL + wiL);
    double cosWoWh = std::max(0.0, glm::dot(woL, wh));

    Color f0 = glm::mix(Color(0.04), p.basecolor, p.metallic);
    Color F = schlickF(f0, cosWoWh);

    double alpha = p.roughness * p.roughness;
    double D = sampling::ggxD(wh, alpha, alpha);
    double G = sampling::ggxG(woL, wiL, alpha, alpha);

    Color diffuse  = (Color(1.0) - F) * (1.0 - p.metallic) * p.basecolor
                   / constants::kPi;
    Color specular = F * (D * G / (4.0 * woL.z * wiL.z));
    Color total = diffuse + specular;
    return total * p.ao;
}

MaterialSample PbrMaterial::sample(const HitRecord& rec,
                                   const glm::dvec3& wo,
                                   const glm::dvec3& u) const {
    Params p = sampleParams(rec);
    Frame f(p.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    MaterialSample s;
    if (woL.z <= 0.0) return s;

    Color f0 = glm::mix(Color(0.04), p.basecolor, p.metallic);
    Color diffuseColor = (1.0 - p.metallic) * p.basecolor;
    double pickSpec = clampPickProb(
        luminance(f0) / (luminance(f0) + luminance(diffuseColor) + 1e-6));

    double alpha = p.roughness * p.roughness;

    glm::dvec3 wiL;
    if (u.z < pickSpec) {
        glm::dvec3 wh = sampling::ggxVndf(woL, alpha, alpha, u.x, u.y);
        wiL = -woL + 2.0 * glm::dot(woL, wh) * wh;
        if (wiL.z <= 0.0) return s;
    } else {
        wiL = sampling::cosineHemisphere(u.x, u.y);
    }

    glm::dvec3 wi = f.toWorld(wiL);
    s.wi = wi;

    glm::dvec3 wh = glm::normalize(woL + wiL);
    double cosWoWh = std::max(0.0, glm::dot(woL, wh));
    Color F = schlickF(f0, cosWoWh);

    double D = sampling::ggxD(wh, alpha, alpha);
    double G = sampling::ggxG(woL, wiL, alpha, alpha);
    double pdfSpec = sampling::ggxVndfPdfReflect(woL, wh, alpha, alpha);
    double pdfDiff = sampling::cosineHemispherePdf(wiL.z);
    double pdfTotal = pickSpec * pdfSpec + (1.0 - pickSpec) * pdfDiff;
    if (pdfTotal <= 0.0) return s;

    Color diffuse  = (Color(1.0) - F) * (1.0 - p.metallic) * p.basecolor
                   / constants::kPi;
    Color specular = F * (D * G / (4.0 * woL.z * wiL.z));
    Color f_value = (diffuse + specular) * p.ao;

    s.pdf = pdfTotal;
    s.weight = f_value * (wiL.z / pdfTotal);
    s.valid = true;
    return s;
}

double PbrMaterial::pdf(const HitRecord& rec,
                        const glm::dvec3& wo,
                        const glm::dvec3& wi) const {
    Params p = sampleParams(rec);
    Frame f(p.shadingNormal);
    glm::dvec3 woL = f.toLocal(wo);
    glm::dvec3 wiL = f.toLocal(wi);
    if (woL.z <= 0.0 || wiL.z <= 0.0) return 0.0;
    Color f0 = glm::mix(Color(0.04), p.basecolor, p.metallic);
    Color diffuseColor = (1.0 - p.metallic) * p.basecolor;
    double pickSpec = clampPickProb(
        luminance(f0) / (luminance(f0) + luminance(diffuseColor) + 1e-6));
    double alpha = p.roughness * p.roughness;
    glm::dvec3 wh = glm::normalize(woL + wiL);
    double pdfSpec = sampling::ggxVndfPdfReflect(woL, wh, alpha, alpha);
    double pdfDiff = sampling::cosineHemispherePdf(wiL.z);
    return pickSpec * pdfSpec + (1.0 - pickSpec) * pdfDiff;
}

namespace {

std::vector<std::string> listFiles(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (auto* e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        out.push_back(name);
    }
    closedir(d);
    return out;
}

std::string findFile(const std::vector<std::string>& files,
                     std::initializer_list<const char*> keys) {
    // earlier keys win, so callers can express preference (e.g. "normalgl"
    // before "normal" to pick the opengl-convention map over the dx one).
    for (auto* key : keys) {
        for (const auto& f : files) {
            if (!isImage(f)) continue;
            std::string lower = f;
            for (auto& c : lower) c = char(std::tolower(c));
            if (lower.find(key) != std::string::npos) return f;
        }
    }
    return {};
}

} // namespace

std::shared_ptr<PbrMaterial> PbrMaterial::loadFromDirectory(
    const std::string& dir,
    const Color& basecolorFactor,
    double roughnessFactor,
    double metallicFactor) {

    auto mat = std::make_shared<PbrMaterial>();
    mat->setBasecolorFactor(basecolorFactor);
    mat->setRoughnessFactor(roughnessFactor);
    mat->setMetallicFactor(metallicFactor);

    auto files = listFiles(dir);
    auto join = [&](const std::string& f) { return dir + "/" + f; };

    std::string base    = findFile(files, {"basecolor", "albedo", "diff", "color"});
    std::string rough   = findFile(files, {"roughness", "rough"});
    std::string metal   = findFile(files, {"metallic", "metalness", "metal"});
    std::string normal  = findFile(files, {"normalgl", "normal", "norm"});
    std::string aoFile  = findFile(files, {"ambientocclusion", "ao", "occlusion"});

    if (!base.empty()) {
        auto t = ImageTexture::load(join(base), /*sRGB=*/true);
        if (t) mat->setBasecolor(t);
        std::cerr << "[pbr] basecolor: " << base << "\n";
    }
    if (!rough.empty()) {
        auto t = ImageTexture::load(join(rough), /*sRGB=*/false);
        if (t) mat->setRoughness(t);
        std::cerr << "[pbr] roughness: " << rough << "\n";
    }
    if (!metal.empty()) {
        auto t = ImageTexture::load(join(metal), /*sRGB=*/false);
        if (t) mat->setMetallic(t);
        std::cerr << "[pbr] metallic: " << metal << "\n";
    } else {
        mat->setMetallicFactor(0.0);  // gltf default for dielectric packs
    }
    if (!normal.empty()) {
        auto t = ImageTexture::load(join(normal), /*sRGB=*/false);
        if (t) mat->setNormal(t);
        std::cerr << "[pbr] normal: " << normal << "\n";
    }
    if (!aoFile.empty()) {
        auto t = ImageTexture::load(join(aoFile), /*sRGB=*/false);
        if (t) mat->setAO(t);
        std::cerr << "[pbr] ao: " << aoFile << "\n";
    }
    return mat;
}
