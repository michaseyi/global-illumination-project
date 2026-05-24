// hdr float framebuffer with qt + png output.

#pragma once

#include <QImage>
#include <string>
#include <vector>

#include "core/color.h"

class Image {
public:
    Image(int width, int height);

    int width() const;
    int height() const;

    void setPixel(int x, int y, const Color& color);

    // accumulator op; call finalize() before reading qimage().
    void addSample(int x, int y, const Color& color);
    void setSampleCount(int spp);
    Color pixel(int x, int y) const;

    const QImage& qimage() const;
    QImage& qimage();

    // rebuild the qimage from the float buffer (tonemap + srgb).
    void finalize();

    bool writePNG(const std::string& path) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_sampleCount = 1;
    std::vector<Color> m_buffer;
    mutable QImage m_qimage;
};
