// QImage-backed image implementation with simple clamping and gamma correction.

#include "io/image.h"

#include <QColor>
#include <algorithm>
#include <cmath>

namespace {
double clamp01(double x) {
    return std::max(0.0, std::min(1.0, x));
}
}

Image::Image(int width, int height)
    : m_image(width, height, QImage::Format_RGB32) {
    m_image.fill(Qt::black);
}

int Image::width() const {
    return m_image.width();
}

int Image::height() const {
    return m_image.height();
}

void Image::setPixel(int x, int y, const Color& color) {
    // Minimal gamma correction from linear RGB to display RGB.
    double r = std::sqrt(clamp01(color.r));
    double g = std::sqrt(clamp01(color.g));
    double b = std::sqrt(clamp01(color.b));

    m_image.setPixelColor(
        x, y,
        QColor(
            static_cast<int>(255.999 * r),
            static_cast<int>(255.999 * g),
            static_cast<int>(255.999 * b)
        )
    );
}

const QImage& Image::qimage() const {
    return m_image;
}