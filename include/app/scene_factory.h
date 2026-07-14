// SceneFactory keeps scene construction out of main.cpp.

#pragma once

#include <memory>
#include <string>

#include "scene/scene.h"
#include "scene/camera.h"
#include "core/color.h"

class Material;
class EnvironmentMap;

struct SceneSetup {
    Scene scene;
    Camera camera;
    Color background{0.0};  // uniform environment radiance for escaped rays
    std::shared_ptr<EnvironmentMap> envMap;  // optional hdr panorama (wins)
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
    Mirror,
    FromFile  // use the materials/textures the .obj's .mtl assigns
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
