// glTF 2.0 loader (.glb / .gltf). unlike OBJ, glTF carries lights, a camera,
// and proper metallic-roughness PBR materials, so a whole scene imports from a
// single file. see gltf_loader.cpp for the supported feature subset.

#pragma once

#include <memory>
#include <string>

#include "app/scene_factory.h"  // SceneSetup

class GltfLoader {
public:
    // parse `path` and build the scene. the camera image size is forced to
    // width x height. `lightScale` multiplies imported punctual-light intensity
    // (glTF stores these in candela/lux, which need scaling to our units).
    // `cameraSelect` picks a camera by node-name substring or numeric index
    // (empty = first camera). returns null on any error.
    // `ceilingLight` (>0) adds a downward area light across the top of the
    // scene's bounding box - a quick way to flood-light an imported room.
    // when `camEye` and `camTarget` are both non-empty ("x,y,z"), they override
    // the file's camera (with `camFov` degrees if > 0) - handy for inspecting an
    // imported scene from an arbitrary angle.
    static std::unique_ptr<SceneSetup> loadFromFile(const std::string& path,
                                                    int width, int height,
                                                    double lightScale = 1.0,
                                                    const std::string& cameraSelect = "",
                                                    double ceilingLight = 0.0,
                                                    const std::string& camEye = "",
                                                    const std::string& camTarget = "",
                                                    double camFov = 0.0);
};
