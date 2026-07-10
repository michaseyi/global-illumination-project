// image texture: stb_image-backed, bilinear-filtered.

#include "shading/image_texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {
double srgbToLinear(double c) {
    if (c <= 0.04045) return c / 12.92;
    return std::pow((c + 0.055) / 1.055, 2.4);
}
} // namespace

std::shared_ptr<ImageTexture> ImageTexture::load(const std::string& path,
                                                 bool sRGB, Wrap wrap) {
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "[image] failed to load " << path << ": "
                  << stbi_failure_reason() << "\n";
        return nullptr;
    }

    auto tex = std::shared_ptr<ImageTexture>(new ImageTexture());
    tex->m_width = w;
    tex->m_height = h;
    tex->m_channels = ch;
    tex->m_sRGB = sRGB;
    tex->m_wrap = wrap;
    tex->m_pixels.resize(size_t(w) * size_t(h) * size_t(ch));
    const double inv = 1.0 / 255.0;
    for (size_t i = 0; i < tex->m_pixels.size(); ++i) {
        tex->m_pixels[i] = float(double(data[i]) * inv);
    }
    stbi_image_free(data);
    return tex;
}

std::shared_ptr<ImageTexture> ImageTexture::fromPixels(const unsigned char* data,
                                                       int width, int height,
                                                       int channels, bool sRGB,
                                                       Wrap wrap) {
    if (!data || width <= 0 || height <= 0 || channels <= 0) return nullptr;
    auto tex = std::shared_ptr<ImageTexture>(new ImageTexture());
    tex->m_width = width;
    tex->m_height = height;
    tex->m_channels = channels;
    tex->m_sRGB = sRGB;
    tex->m_wrap = wrap;
    tex->m_pixels.resize(size_t(width) * size_t(height) * size_t(channels));
    const double inv = 1.0 / 255.0;
    for (size_t i = 0; i < tex->m_pixels.size(); ++i) {
        tex->m_pixels[i] = float(double(data[i]) * inv);
    }
    return tex;
}

std::shared_ptr<ImageTexture> ImageTexture::loadHDR(const std::string& path,
                                                    Wrap wrap) {
    int w = 0, h = 0, ch = 0;
    float* data = stbi_loadf(path.c_str(), &w, &h, &ch, 3);
    if (!data) {
        std::cerr << "[image] failed to load hdr " << path << ": "
                  << stbi_failure_reason() << "\n";
        return nullptr;
    }
    auto tex = std::shared_ptr<ImageTexture>(new ImageTexture());
    tex->m_width = w;
    tex->m_height = h;
    tex->m_channels = 3;
    tex->m_sRGB = false;  // radiance values are already linear
    tex->m_wrap = wrap;
    tex->m_pixels.assign(data, data + size_t(w) * h * 3);
    stbi_image_free(data);
    return tex;
}

int ImageTexture::wrapX(int x) const {
    if (m_wrap == Wrap::Repeat) {
        int m = x % m_width;
        if (m < 0) m += m_width;
        return m;
    }
    return std::max(0, std::min(m_width - 1, x));
}

int ImageTexture::wrapY(int y) const {
    if (m_wrap == Wrap::Repeat) {
        int m = y % m_height;
        if (m < 0) m += m_height;
        return m;
    }
    return std::max(0, std::min(m_height - 1, y));
}

glm::dvec4 ImageTexture::fetch(int x, int y) const {
    int ix = wrapX(x);
    int iy = wrapY(y);
    size_t off = (size_t(iy) * size_t(m_width) + size_t(ix)) * size_t(m_channels);
    glm::dvec4 c(0.0, 0.0, 0.0, 1.0);
    if (m_channels >= 1) c.r = m_pixels[off + 0];
    if (m_channels >= 2) c.g = m_pixels[off + 1];
    else                 c.g = c.r;
    if (m_channels >= 3) c.b = m_pixels[off + 2];
    else                 c.b = c.r;
    if (m_channels >= 4) c.a = m_pixels[off + 3];
    if (m_sRGB) {
        c.r = srgbToLinear(c.r);
        c.g = srgbToLinear(c.g);
        c.b = srgbToLinear(c.b);
    }
    return c;
}

glm::dvec4 ImageTexture::sampleRGBA(const glm::dvec2& uv) const {
    if (m_pixels.empty()) return glm::dvec4(0.0, 0.0, 0.0, 1.0);
    // map (u, v) -> texel space using top-left origin and inverted v
    // (most image formats store top-row first).
    double u = uv.x;
    double v = 1.0 - uv.y;
    double x = u * double(m_width)  - 0.5;
    double y = v * double(m_height) - 0.5;
    int x0 = int(std::floor(x));
    int y0 = int(std::floor(y));
    double fx = x - double(x0);
    double fy = y - double(y0);

    glm::dvec4 c00 = fetch(x0,     y0);
    glm::dvec4 c10 = fetch(x0 + 1, y0);
    glm::dvec4 c01 = fetch(x0,     y0 + 1);
    glm::dvec4 c11 = fetch(x0 + 1, y0 + 1);
    glm::dvec4 a = c00 * (1.0 - fx) + c10 * fx;
    glm::dvec4 b = c01 * (1.0 - fx) + c11 * fx;
    return a * (1.0 - fy) + b * fy;
}

Color ImageTexture::value(const glm::dvec2& uv, const glm::dvec3&) const {
    glm::dvec4 c = sampleRGBA(uv);
    return Color(c.r, c.g, c.b);
}
