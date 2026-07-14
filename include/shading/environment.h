// image-based environment: an equirectangular (lat-long) hdr panorama that
// supplies radiance for rays that leave the scene. this is what puts a sky /
// landscape behind windows and gives image-based ambient lighting, instead of
// the flat --bg color.

#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/color.h"

class ImageTexture;

class EnvironmentMap {
public:
    // load an equirect .hdr panorama. `scale` multiplies the radiance,
    // `rotationDeg` spins the panorama around +Y. null on failure.
    static std::shared_ptr<EnvironmentMap> load(const std::string& path,
                                                double scale = 1.0,
                                                double rotationDeg = 0.0);

    // radiance arriving from direction `dir` (world space, any length > 0).
    Color radiance(const glm::dvec3& dir) const;

private:
    EnvironmentMap() = default;

    std::shared_ptr<ImageTexture> m_tex;
    double m_scale = 1.0;
    double m_rotation = 0.0;  // radians
};
