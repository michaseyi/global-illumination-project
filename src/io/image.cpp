// hdr framebuffer with tonemap + png export.

#include "io/image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <QColor>
#include <algorithm>
#include <cmath>

namespace {
double clamp01(double x) {
    return std::max(0.0, std::min(1.0, x));
}

// aces filmic approximation by krzysztof narkowicz
Color acesTonemap(const Color& x) {
    const double a = 2.51;
    const double b = 0.03;
    const double c = 2.43;
    const double d = 0.59;
    const double e = 0.14;
    Color num = x * (a * x + Color(b));
    Color den = x * (c * x + Color(d)) + Color(e);
    return Color(clamp01(num.r / den.r),
                 clamp01(num.g / den.g),
                 clamp01(num.b / den.b));
}

double linearToSrgb(double c) {
    c = clamp01(c);
    return c <= 0.0031308
        ? 12.92 * c
        : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}
} // namespace

Image::Image(int width, int height)
    : m_width(width),
      m_height(height),
      m_buffer(width * height, Color(0.0)),
      m_qimage(width, height, QImage::Format_RGB32) {
    m_qimage.fill(Qt::black);
}

int Image::width() const { return m_width; }
int Image::height() const { return m_height; }

void Image::setPixel(int x, int y, const Color& color) {
    m_buffer[y * m_width + x] = color;
}

void Image::addSample(int x, int y, const Color& color) {
    m_buffer[y * m_width + x] += color;
}

void Image::setSampleCount(int spp) {
    m_sampleCount = spp > 0 ? spp : 1;
}

Color Image::pixel(int x, int y) const {
    return m_buffer[y * m_width + x] / static_cast<double>(m_sampleCount);
}

const QImage& Image::qimage() const { return m_qimage; }
QImage& Image::qimage() { return m_qimage; }

void Image::finalize() {
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            Color c = pixel(x, y);
            // guard against nan/inf seeping in
            if (!(c.r == c.r)) c.r = 0.0;
            if (!(c.g == c.g)) c.g = 0.0;
            if (!(c.b == c.b)) c.b = 0.0;
            c = acesTonemap(c);
            int r = static_cast<int>(255.0 * linearToSrgb(c.r) + 0.5);
            int g = static_cast<int>(255.0 * linearToSrgb(c.g) + 0.5);
            int b = static_cast<int>(255.0 * linearToSrgb(c.b) + 0.5);
            m_qimage.setPixelColor(x, y,
                QColor(std::max(0, std::min(255, r)),
                       std::max(0, std::min(255, g)),
                       std::max(0, std::min(255, b))));
        }
    }
}

bool Image::writePNG(const std::string& path) const {
    std::vector<unsigned char> bytes(m_width * m_height * 3);
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            Color c = pixel(x, y);
            if (!(c.r == c.r)) c.r = 0.0;
            if (!(c.g == c.g)) c.g = 0.0;
            if (!(c.b == c.b)) c.b = 0.0;
            c = acesTonemap(c);
            size_t off = 3 * (y * m_width + x);
            bytes[off + 0] = static_cast<unsigned char>(
                std::max(0, std::min(255, int(255.0 * linearToSrgb(c.r) + 0.5))));
            bytes[off + 1] = static_cast<unsigned char>(
                std::max(0, std::min(255, int(255.0 * linearToSrgb(c.g) + 0.5))));
            bytes[off + 2] = static_cast<unsigned char>(
                std::max(0, std::min(255, int(255.0 * linearToSrgb(c.b) + 0.5))));
        }
    }
    return stbi_write_png(path.c_str(), m_width, m_height, 3,
                          bytes.data(), m_width * 3) != 0;
}
