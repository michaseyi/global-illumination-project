// Main application window: toolbar + rendered image view.

#pragma once

#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QToolBar>
#include <QWidget>
#include <QWindow>

#include "app/viewer.h"
#include "render/integrator.h"
#include "scene/camera.h"
#include "scene/scene.h"

class Gui : public QMainWindow {
public:
    Gui() = delete;

    // samplesPerPixel is forwarded to the Viewer/Renderer pipeline.
    Gui(int width,
        int height,
        const Scene& scene,
        const Camera& camera,
        const Integrator& integrator,
        int samplesPerPixel,
        QWindow* parent = nullptr);

    ~Gui();

private:
    QLabel* m_durationText = nullptr;
    QPushButton* m_saveButton = nullptr;
    Viewer* m_viewer = nullptr;
};
