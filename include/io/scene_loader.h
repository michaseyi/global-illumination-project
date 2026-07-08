// build a SceneSetup from a json scene description, so scenes can be composed
// and tweaked without recompiling. see assets/scenes/*.json for the format and
// scene_loader.cpp for the full schema.

#pragma once

#include <memory>
#include <string>

#include "app/scene_factory.h"  // SceneSetup

class SceneLoader {
public:
    // parse `path` and build the scene. camera image size is forced to
    // width x height (aspect from the file's fov). returns null on any parse
    // or schema error, printing the reason to stderr.
    static std::unique_ptr<SceneSetup> loadFromFile(const std::string& path,
                                                    int width, int height);
};
