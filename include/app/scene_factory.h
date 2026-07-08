// SceneFactory keeps scene construction out of main.cpp.

#pragma once

#include <memory>
#include <string>

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

// surface look applied to a loaded mesh in createMeshScene().
enum class MeshMaterial {
    Diffuse,
    Metal,
    Glass,
    Mirror
};

class SceneFactory {
public:
    static SceneSetup createStarterScene(int width, int height);

    // classic cornell box. if `sphereOverride` is non-null, it replaces the
    // material assigned by `kind` on the right-hand sphere.
    static SceneSetup createCornellBoxScene(int width, int height,
                                            CornellSphere kind = CornellSphere::Glass,
                                            std::shared_ptr<Material> sphereOverride = nullptr);

    // cornell-box shell (colored walls + ceiling area light) with an OBJ mesh
    // auto-fitted onto the floor. great for showing off mesh loading with
    // real global illumination (color bleeding onto the model).
    static SceneSetup createMeshScene(int width, int height,
                                      const std::string& objPath,
                                      MeshMaterial kind = MeshMaterial::Diffuse);

    // cornell-style box with two glass spheres and a mirror sphere — shows
    // off refraction and reflection together.
    static SceneSetup createGlassShowcaseScene(int width, int height);
};
