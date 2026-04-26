// Widget that triggers one render and displays the resulting image.

#pragma once

#include <QImage>
#include <QLabel>
#include <QWidget>

#include "render/integrator.h"
#include "scene/camera.h"
#include "scene/scene.h"

class Viewer : public QWidget {
public:
    Viewer(int width,
           int height,
           const Scene& scene,
           const Camera& camera,
           const Integrator& integrator,
           int samplesPerPixel,
           QLabel* durationLabel,
           QWidget* parent = nullptr);

    const QImage& getImage() const;

    // Kept for compatibility with future threaded/progressive viewers.
    void stopRaytrace();

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    // Render once into m_image.
    void renderScene();

private:
    int m_width = 0;
    int m_height = 0;

    const Scene& m_scene;
    const Camera& m_camera;
    const Integrator& m_integrator;
    int m_samplesPerPixel = 1;

    QLabel* m_durationLabel = nullptr;
    QImage m_image;
};
