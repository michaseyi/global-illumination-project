// SceneFactory keeps scene construction out of main.cpp.

#pragma once

#include <memory>

#include "scene/scene.h"
#include "scene/camera.h"

class Material;

struct SceneSetup {
    Scene scene;
    Camera camera;
};

enum class CornellSphere {
    Glass,
    RoughMetal,
    Diffuse
};

class SceneFactory {
public:
    static SceneSetup createStarterScene(int width, int height);

    // classic cornell box. if `sphereOverride` is non-null, it replaces the
    // material assigned by `kind` on the right-hand sphere.
    static SceneSetup createCornellBoxScene(int width, int height,
                                            CornellSphere kind = CornellSphere::Glass,
                                            std::shared_ptr<Material> sphereOverride = nullptr);
};
