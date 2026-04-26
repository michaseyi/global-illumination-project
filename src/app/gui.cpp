// Implementation of the main GUI window.
// The viewer renders once during construction and the toolbar provides
// a save button plus the measured render time.

#include "app/gui.h"

Gui::Gui(int width,
         int height,
         const Scene& scene,
         const Camera& camera,
         const Integrator& integrator,
         int samplesPerPixel,
         QWindow* parent)
    : QMainWindow(nullptr) {
    Q_UNUSED(parent);

    QToolBar* toolbar = new QToolBar(this);
    toolbar->setMovable(false);

    m_saveButton = new QPushButton("Save as ...", this);
    toolbar->addWidget(m_saveButton);

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar->addWidget(spacer);

    m_durationText = new QLabel(this);
    m_durationText->setText("Rendering...");
    toolbar->addWidget(m_durationText);

    addToolBar(toolbar);

    m_viewer = new Viewer(
        width,
        height,
        scene,
        camera,
        integrator,
        samplesPerPixel,
        m_durationText,
        this
    );
    m_viewer->resize(width, height);
    setCentralWidget(m_viewer);

    connect(m_saveButton, &QPushButton::clicked, [this]() {
        QString filename = QFileDialog::getSaveFileName(
            this,
            tr("Save Image"),
            "render.png",
            tr("Images (*.png);;All Files (*)")
        );

        if (filename.isEmpty()) {
            return;
        }

        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            m_viewer->getImage().save(&file, "PNG");
        }
    });

    resize(width, height);
}

Gui::~Gui() {
    if (m_viewer) {
        m_viewer->stopRaytrace();
    }
}
