// bilinear-filtered image texture (png/jpg/hdr via stb_image).
// supports an sRGB decode flag (true for basecolor, false for data maps).

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "shading/texture.h"

class ImageTexture : public Texture {
public:
    enum class Wrap { Repeat, Clamp };

    // load `path`. returns null on failure (prints to stderr).
    static std::shared_ptr<ImageTexture> load(const std::string& path,
                                              bool sRGB = false,
                                              Wrap wrap = Wrap::Repeat);

    Color value(const glm::dvec2& uv,
                const glm::dvec3& position) const override;

    // raw rgba sampler returning the 4th channel as 1 when missing.
    glm::dvec4 sampleRGBA(const glm::dvec2& uv) const;

    int width() const  { return m_width; }
    int height() const { return m_height; }

private:
    ImageTexture() = default;

    glm::dvec4 fetch(int x, int y) const;
    int wrapX(int x) const;
    int wrapY(int y) const;

    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    bool m_sRGB = false;
    Wrap m_wrap = Wrap::Repeat;
    std::vector<float> m_pixels;  // interleaved rgba (or rgb)
};
