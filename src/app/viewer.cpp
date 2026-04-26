// Viewer implementation for the minimal starter.
// The render is done once in the constructor, and the resulting QImage is drawn.

#include "app/viewer.h"

#include <QPainter>
#include <chrono>

#include "io/image.h"
#include "render/renderer.h"

Viewer::Viewer(int width,
               int height,
               const Scene& scene,
               const Camera& camera,
               const Integrator& integrator,
               int samplesPerPixel,
               QLabel* durationLabel,
               QWidget* parent)
    : QWidget(parent),
      m_width(width),
      m_height(height),
      m_scene(scene),
      m_camera(camera),
      m_integrator(integrator),
      m_samplesPerPixel(samplesPerPixel),
      m_durationLabel(durationLabel),
      m_image(width, height, QImage::Format_RGB32) {
    renderScene();
}

void Viewer::renderScene() {
    auto start = std::chrono::high_resolution_clock::now();

    Image image(m_width, m_height);
    Renderer renderer(m_samplesPerPixel);
    renderer.render(m_scene, m_camera, m_integrator, image);

    m_image = image.qimage();

    auto end = std::chrono::high_resolution_clock::now();
    auto durationMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (m_durationLabel) {
        m_durationLabel->setText(
            QString("Render time: %1 ms (%2 spp)")
                .arg(durationMs)
                .arg(m_samplesPerPixel)
        );
    }

    update();
}

const QImage& Viewer::getImage() const {
    return m_image;
}

void Viewer::stopRaytrace() {
    // No background rendering thread is used in the minimal starter.
    // This method is kept for interface compatibility and future extension.
}

void Viewer::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!m_image.isNull()) {
        painter.drawImage(rect(), m_image);
    }
}

QSize Viewer::sizeHint() const {
    return QSize(m_width, m_height);
}
