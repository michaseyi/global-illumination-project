// wavefront .obj loader, tinyobjloader-backed.

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "io/obj_loader.h"

#include <iostream>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

#include "geometry/triangle.h"
#include "scene/scene.h"
#include "shading/material.h"

bool ObjLoader::load(const std::string& path,
                     Scene& scene,
                     std::shared_ptr<Material> fallbackMaterial,
                     const glm::dmat4& transform) const {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> mats;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &mats, &warn, &err,
                               path.c_str());
    if (!warn.empty()) std::cerr << "[obj warn] " << warn;
    if (!err.empty())  std::cerr << "[obj err] " << err;
    if (!ok) return false;

    glm::dmat3 normalMatrix = glm::dmat3(glm::transpose(glm::inverse(transform)));

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv < 3) { indexOffset += fv; continue; }

            std::vector<glm::dvec3> pos(fv);
            std::vector<glm::dvec3> nor(fv);
            std::vector<glm::dvec2> uvs(fv);
            bool hasNormals = true;
            bool hasUVs = true;
            for (int v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                pos[v] = glm::dvec3(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]);
                pos[v] = glm::dvec3(transform * glm::dvec4(pos[v], 1.0));

                if (idx.normal_index >= 0) {
                    glm::dvec3 n(
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]);
                    nor[v] = glm::normalize(normalMatrix * n);
                } else {
                    hasNormals = false;
                }

                if (idx.texcoord_index >= 0) {
                    uvs[v] = glm::dvec2(
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        attrib.texcoords[2 * idx.texcoord_index + 1]);
                } else {
                    hasUVs = false;
                }
            }

            for (int t = 1; t + 1 < fv; ++t) {
                if (hasNormals && hasUVs) {
                    scene.addPrimitive(std::make_shared<Triangle>(
                        pos[0], pos[t], pos[t + 1],
                        nor[0], nor[t], nor[t + 1],
                        uvs[0], uvs[t], uvs[t + 1],
                        fallbackMaterial));
                } else if (hasNormals) {
                    scene.addPrimitive(std::make_shared<Triangle>(
                        pos[0], pos[t], pos[t + 1],
                        nor[0], nor[t], nor[t + 1],
                        fallbackMaterial));
                } else {
                    scene.addPrimitive(std::make_shared<Triangle>(
                        pos[0], pos[t], pos[t + 1], fallbackMaterial));
                }
            }
            indexOffset += fv;
        }
    }
    return true;
}
