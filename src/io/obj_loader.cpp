// wavefront .obj loader, tinyobjloader-backed.
//
// pipeline placement: this sits in io/ and feeds geometry/ — it converts a
// mesh file into many Triangle primitives and hands them to the Scene, which
// then builds its BVH over them. two quality-of-life features beyond raw
// loading: (1) when a file ships without vertex normals (common for the
// classic teapot/bunny test models) we synthesize smooth shading normals so
// the surface looks curved rather than faceted; (2) loadFitted() auto-scales
// and positions an arbitrary model so it drops cleanly into a scene.

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "io/obj_loader.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "geometry/triangle.h"
#include "scene/scene.h"
#include "shading/material.h"

namespace {

struct MeshData {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
};

bool allFinite(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool parseObj(const std::string& path, MeshData& out) {
    std::vector<tinyobj::material_t> mats;
    std::string warn, err;
    bool ok = tinyobj::LoadObj(&out.attrib, &out.shapes, &mats,
                               &warn, &err, path.c_str());
    if (!warn.empty()) std::cerr << "[obj warn] " << warn;
    if (!err.empty())  std::cerr << "[obj err] "  << err;
    return ok;
}

// build per-vertex smooth normals by accumulating each triangle's face normal
// (left un-normalized, so larger faces contribute proportionally to area) onto
// its three vertices, then normalizing. world-space positions are used so the
// generated normals are already in the final frame.
std::vector<glm::dvec3> generateSmoothNormals(
        const MeshData& mesh,
        const std::vector<glm::dvec3>& worldPos) {
    std::vector<glm::dvec3> normals(worldPos.size(), glm::dvec3(0.0));
    for (const auto& shape : mesh.shapes) {
        size_t off = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv >= 3) {
                int i0 = shape.mesh.indices[off + 0].vertex_index;
                for (int t = 1; t + 1 < fv; ++t) {
                    int i1 = shape.mesh.indices[off + t].vertex_index;
                    int i2 = shape.mesh.indices[off + t + 1].vertex_index;
                    if (i0 < 0 || i1 < 0 || i2 < 0) continue;
                    glm::dvec3 fn = glm::cross(worldPos[i1] - worldPos[i0],
                                               worldPos[i2] - worldPos[i0]);
                    normals[i0] += fn;
                    normals[i1] += fn;
                    normals[i2] += fn;
                }
            }
            off += static_cast<size_t>(fv);
        }
    }
    for (auto& n : normals) {
        double len = glm::length(n);
        n = (len > 1e-20) ? n / len : glm::dvec3(0.0, 1.0, 0.0);
    }
    return normals;
}

// turn a parsed mesh into Triangle primitives in the scene.
void emitTriangles(const MeshData& mesh,
                   Scene& scene,
                   const std::shared_ptr<Material>& material,
                   const glm::dmat4& transform) {
    const auto& attrib = mesh.attrib;
    const size_t vCount = attrib.vertices.size() / 3;
    if (vCount == 0) return;

    // transform each unique vertex once.
    std::vector<glm::dvec3> worldPos(vCount);
    for (size_t i = 0; i < vCount; ++i) {
        glm::dvec4 p(attrib.vertices[3 * i + 0],
                     attrib.vertices[3 * i + 1],
                     attrib.vertices[3 * i + 2], 1.0);
        worldPos[i] = glm::dvec3(transform * p);
    }

    const bool fileHasNormals = !attrib.normals.empty();
    const bool fileHasUVs     = !attrib.texcoords.empty();
    glm::dmat3 normalMatrix =
        glm::dmat3(glm::transpose(glm::inverse(transform)));

    std::vector<glm::dvec3> smooth;
    if (!fileHasNormals) smooth = generateSmoothNormals(mesh, worldPos);

    // all index accesses are upper-bound checked: tinyobjloader validates that
    // indices are non-negative but NOT that they fit the attribute arrays, so a
    // malformed file could otherwise read out of bounds.
    auto normalAt = [&](const tinyobj::index_t& idx) -> glm::dvec3 {
        if (fileHasNormals && idx.normal_index >= 0 &&
            static_cast<size_t>(3 * idx.normal_index + 3) <= attrib.normals.size()) {
            glm::dvec3 n(attrib.normals[3 * idx.normal_index + 0],
                         attrib.normals[3 * idx.normal_index + 1],
                         attrib.normals[3 * idx.normal_index + 2]);
            return glm::normalize(normalMatrix * n);
        }
        if (!smooth.empty() && idx.vertex_index >= 0 &&
            static_cast<size_t>(idx.vertex_index) < smooth.size()) {
            return smooth[idx.vertex_index];
        }
        return glm::dvec3(0.0, 1.0, 0.0);
    };
    auto uvAt = [&](const tinyobj::index_t& idx) -> glm::dvec2 {
        if (fileHasUVs && idx.texcoord_index >= 0 &&
            static_cast<size_t>(2 * idx.texcoord_index + 2) <= attrib.texcoords.size()) {
            return glm::dvec2(attrib.texcoords[2 * idx.texcoord_index + 0],
                              attrib.texcoords[2 * idx.texcoord_index + 1]);
        }
        return glm::dvec2(0.0);
    };

    for (const auto& shape : mesh.shapes) {
        size_t off = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv < 3) { off += static_cast<size_t>(fv); continue; }

            const tinyobj::index_t v0 = shape.mesh.indices[off + 0];
            const bool faceHasUV = fileHasUVs && v0.texcoord_index >= 0;

            // fan-triangulate the (already convex) polygon.
            for (int t = 1; t + 1 < fv; ++t) {
                const tinyobj::index_t v1 = shape.mesh.indices[off + t];
                const tinyobj::index_t v2 = shape.mesh.indices[off + t + 1];

                // drop any face referencing an out-of-range vertex.
                if (v0.vertex_index < 0 || v1.vertex_index < 0 || v2.vertex_index < 0 ||
                    static_cast<size_t>(v0.vertex_index) >= worldPos.size() ||
                    static_cast<size_t>(v1.vertex_index) >= worldPos.size() ||
                    static_cast<size_t>(v2.vertex_index) >= worldPos.size()) {
                    continue;
                }

                glm::dvec3 p0 = worldPos[v0.vertex_index];
                glm::dvec3 p1 = worldPos[v1.vertex_index];
                glm::dvec3 p2 = worldPos[v2.vertex_index];
                // skip non-finite triangles so NaN/Inf can't poison the BVH.
                if (!allFinite(p0) || !allFinite(p1) || !allFinite(p2)) continue;
                glm::dvec3 n0 = normalAt(v0);
                glm::dvec3 n1 = normalAt(v1);
                glm::dvec3 n2 = normalAt(v2);

                if (faceHasUV) {
                    scene.addPrimitive(std::make_shared<Triangle>(
                        p0, p1, p2, n0, n1, n2,
                        uvAt(v0), uvAt(v1), uvAt(v2), material));
                } else {
                    scene.addPrimitive(std::make_shared<Triangle>(
                        p0, p1, p2, n0, n1, n2, material));
                }
            }
            off += static_cast<size_t>(fv);
        }
    }
}

// uniform scale + translate that fits the mesh's object-space AABB into a cube
// of side `targetSize` centered at `center` (optionally resting on the floor).
glm::dmat4 computeFitTransform(const MeshData& mesh,
                               const glm::dvec3& center,
                               double targetSize,
                               bool sitOnGround) {
    const auto& v = mesh.attrib.vertices;
    glm::dvec3 lo( std::numeric_limits<double>::infinity());
    glm::dvec3 hi(-std::numeric_limits<double>::infinity());
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
        glm::dvec3 p(v[i], v[i + 1], v[i + 2]);
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    glm::dvec3 ext = hi - lo;
    double maxExt = std::max(ext.x, std::max(ext.y, ext.z));
    double scale  = (maxExt > 1e-12) ? targetSize / maxExt : 1.0;
    glm::dvec3 objCenter = 0.5 * (lo + hi);

    // worldPos = scale * objPos + translate
    glm::dvec3 translate = center - scale * objCenter;
    if (sitOnGround) {
        // pin the base (min-y) to the floor plane y = center.y
        translate.y = center.y - scale * lo.y;
    }

    glm::dmat4 M(1.0);
    M = glm::translate(M, translate);
    M = glm::scale(M, glm::dvec3(scale));
    return M;
}

} // namespace

bool ObjLoader::load(const std::string& path,
                     Scene& scene,
                     std::shared_ptr<Material> fallbackMaterial,
                     const glm::dmat4& transform) const {
    MeshData mesh;
    if (!parseObj(path, mesh)) return false;
    emitTriangles(mesh, scene, fallbackMaterial, transform);
    return true;
}

bool ObjLoader::loadFitted(const std::string& path,
                           Scene& scene,
                           std::shared_ptr<Material> fallbackMaterial,
                           const glm::dvec3& center,
                           double targetSize,
                           bool sitOnGround) const {
    MeshData mesh;
    if (!parseObj(path, mesh)) return false;
    glm::dmat4 M = computeFitTransform(mesh, center, targetSize, sitOnGround);
    emitTriangles(mesh, scene, fallbackMaterial, M);
    return true;
}
