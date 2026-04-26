// Image wrapper around QImage.
// This keeps Qt-specific image handling out of the rendering core.

#pragma once

#include <QImage>
#include "core/color.h"

class Image {
public:
    Image(int width, int height);

    int width() const;
    int height() const;

    void setPixel(int x, int y, const Color& color);
    const QImage& qimage() const;

private:
    QImage m_image;
};