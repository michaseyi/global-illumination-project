// Placeholder interface for future mesh loading work.

#pragma once

#include <string>

class Scene;

class ObjLoader {
public:
    // Returns false in the starter until OBJ loading is implemented.
    bool load(const std::string& path, Scene& scene) const;
};
