// wavefront .obj loader (header for tinyobjloader-backed implementation).

#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

class Scene;
class Material;

class ObjLoader {
public:
    // load `path` into `scene` as triangles using `fallbackMaterial`.
    // `transform` is applied to vertex positions before insertion.
    bool load(const std::string& path,
              Scene& scene,
              std::shared_ptr<Material> fallbackMaterial,
              const glm::dmat4& transform = glm::dmat4(1.0)) const;
};
