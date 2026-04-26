// SceneFactory keeps scene construction out of main.cpp.

#pragma once

#include "scene/scene.h"
#include "scene/camera.h"

struct SceneSetup {
    Scene scene;
    Camera camera;
};

class SceneFactory {
public:
    // Minimal starter scene:
    // two spheres + one triangle floor + one point light
    static SceneSetup createStarterScene(int width, int height);

    // Extend here with more reusable scenes:
    // static SceneSetup createShadowTestScene(int width, int height);
    // static SceneSetup createReflectionTestScene(int width, int height);
    // static SceneSetup createCornellBoxScene(int width, int height);
};
