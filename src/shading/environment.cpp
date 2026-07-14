#include "shading/environment.h"

#include <cmath>
#include <iostream>

#include "core/constants.h"
#include "shading/image_texture.h"

std::shared_ptr<EnvironmentMap> EnvironmentMap::load(const std::string& path,
                                                     double scale,
                                                     double rotationDeg) {
    auto tex = ImageTexture::loadHDR(path);
    if (!tex) return nullptr;
    auto env = std::shared_ptr<EnvironmentMap>(new EnvironmentMap());
    env->m_tex = tex;
    env->m_scale = scale;
    env->m_rotation = rotationDeg * constants::kPi / 180.0;
    std::cerr << "[env] " << path << " (" << tex->width() << "x"
              << tex->height() << "), scale " << scale << "\n";
    return env;
}

Color EnvironmentMap::radiance(const glm::dvec3& dir) const {
    glm::dvec3 d = glm::normalize(dir);
    // equirect longitude, matching the convention panoramas are authored in
    // (blender/poly haven). the argument order matters: atan2(x, z) keeps the
    // view un-mirrored - atan2(z, x) renders the panorama flipped left/right
    // (verified via sun position vs shadow direction).
    double phi = std::atan2(d.x, d.z) + m_rotation;
    double u = phi / (2.0 * constants::kPi) + 0.5;
    u -= std::floor(u);
    // ImageTexture flips v (obj convention), so hand it 1-v to land the zenith
    // (acos = 0) on the panorama's top row.
    double v = 1.0 - std::acos(std::max(-1.0, std::min(1.0, d.y))) / constants::kPi;
    return m_tex->value(glm::dvec2(u, v), d) * m_scale;
}
