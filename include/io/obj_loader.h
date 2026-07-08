// wavefront .obj loader (header for tinyobjloader-backed implementation).

#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

class Scene;
class Material;

class ObjLoader {
public:
    // load `path` into `scene` as triangles. `transform` is applied to vertex
    // positions before insertion. when the file has no vertex normals, smooth
    // normals are generated automatically (area-weighted average of adjacent
    // face normals).
    //
    // when `preferFileMaterials` is true and the .obj references a .mtl, each
    // face is shaded with the material the file assigns it (colors, map_Kd
    // textures, emission, transparency, metalness — see obj_loader.cpp for the
    // mapping); faces without a material fall back to `fallbackMaterial`. when
    // false, every triangle uses `fallbackMaterial`.
    bool load(const std::string& path,
              Scene& scene,
              std::shared_ptr<Material> fallbackMaterial,
              const glm::dmat4& transform = glm::dmat4(1.0),
              bool preferFileMaterials = false) const;

    // convenience loader that auto-fits an arbitrary mesh into the scene:
    // the mesh is uniformly scaled so its largest dimension equals
    // `targetSize`, then placed at `center`. when `sitOnGround` is true the
    // mesh is shifted so its base rests on the plane y = center.y (handy for
    // dropping a model onto a floor). normals are generated if absent.
    // `preferFileMaterials` behaves as in load().
    bool loadFitted(const std::string& path,
                    Scene& scene,
                    std::shared_ptr<Material> fallbackMaterial,
                    const glm::dvec3& center,
                    double targetSize,
                    bool sitOnGround = true,
                    bool preferFileMaterials = false) const;
};
