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
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "geometry/triangle.h"
#include "scene/scene.h"
#include "shading/material.h"
#include "shading/image_texture.h"
#include "shading/pbr_material.h"

#include <cctype>

namespace {

struct MeshData {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string baseDir;  // directory of the .obj, for resolving textures
};

// directory portion of a path, including the trailing slash ("" if none).
std::string dirOf(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? std::string() : path.substr(0, s + 1);
}

bool allFinite(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool parseObj(const std::string& path, MeshData& out) {
    out.baseDir = dirOf(path);
    std::string warn, err;
    // pass the .obj's directory as mtl_basedir so the companion .mtl (and, in
    // turn, its texture paths) resolve relative to the model file.
    bool ok = tinyobj::LoadObj(&out.attrib, &out.shapes, &out.materials,
                               &warn, &err, path.c_str(),
                               out.baseDir.empty() ? nullptr : out.baseDir.c_str());
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

using TexCache = std::map<std::string, std::shared_ptr<ImageTexture>>;

Color toColor(const tinyobj::real_t c[3]) { return Color(c[0], c[1], c[2]); }
bool anyPositive(const Color& c) { return (c.r + c.g + c.b) > 1e-4; }

bool fileExists(const std::string& p) {
    std::ifstream f(p.c_str());
    return f.good();
}

std::string baseName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return (s == std::string::npos) ? p : p.substr(s + 1);
}

// resolve `name` against the model directory and load it (cached, so a texture
// shared by several materials is read once). caches null on failure to avoid
// retrying a missing file. `sRGB` should be true for color maps like map_Kd.
//
// materials commonly reference a bare filename while shipping the images in a
// sibling textures/ folder, so we probe a few candidate locations and use the
// first that exists.
std::shared_ptr<ImageTexture> loadTexture(TexCache& cache,
                                          const std::string& baseDir,
                                          const std::string& name,
                                          bool sRGB) {
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;

    std::string rel = name;
    for (auto& ch : rel) if (ch == '\\') ch = '/';  // normalize windows paths
    std::string bn = baseName(rel);

    std::vector<std::string> candidates;
    if (!rel.empty() && rel[0] == '/') candidates.push_back(rel);  // absolute
    candidates.push_back(baseDir + rel);              // as authored
    candidates.push_back(baseDir + "textures/" + bn); // sibling textures/ dir
    candidates.push_back(baseDir + bn);               // flat next to the model

    std::string found;
    for (const auto& c : candidates) {
        if (fileExists(c)) { found = c; break; }
    }

    std::shared_ptr<ImageTexture> tex;
    if (found.empty()) {
        std::cerr << "[obj] texture not found: " << name << "\n";
    } else {
        tex = ImageTexture::load(found, sRGB, ImageTexture::Wrap::Repeat);
    }
    cache[name] = tex;
    return tex;
}

std::string toLower(std::string s) {
    for (auto& c : s) c = char(std::tolower((unsigned char)c));
    return s;
}
bool nameHas(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// roughness for a material with no roughness map. blender-style OBJ exports set
// a high default Ns (specular exponent) on nearly everything, which naively maps
// to a near-mirror roughness and turns matte surfaces into glossy mirrors that
// reflect the environment instead of showing their diffuse color. so we ignore
// Ns unless it is genuinely low (a deliberately shiny material) and otherwise
// default to a moderate, mostly-diffuse roughness.
double roughnessFromNs(double ns) {
    if (ns <= 0.0) return 0.6;
    double r = std::sqrt(2.0 / (ns + 2.0));
    return std::max(0.45, std::min(1.0, r));
}

// assemble a metallic-roughness PbrMaterial from a .mtl entry.
//
// exporters routinely stash roughness/metallic/normal maps in whatever mtl slot
// is handy (map_Ns, map_refl, map_Bump) with no PBR scalars, so we classify each
// referenced image by filename keyword first and fall back to the slot. that
// makes a metal render glossy (metallic + roughness) with surface relief (normal
// map) instead of a flat basecolor.
std::shared_ptr<Material> buildPbrMaterial(const tinyobj::material_t& m,
                                           const std::string& baseDir,
                                           TexCache& cache) {
    std::string baseN = m.diffuse_texname, roughN, metalN, normN;
    const std::string all[] = {
        m.diffuse_texname, m.specular_texname, m.specular_highlight_texname,
        m.bump_texname, m.reflection_texname, m.normal_texname,
        m.roughness_texname, m.metallic_texname
    };
    for (const auto& n : all) {
        if (n.empty()) continue;
        std::string l = toLower(baseName(n));
        if (nameHas(l, "normal") || nameHas(l, "_norm") || nameHas(l, "nrm")) {
            if (normN.empty()) normN = n;
        } else if (nameHas(l, "rough")) {
            if (roughN.empty()) roughN = n;
        } else if (nameHas(l, "metal")) {
            if (metalN.empty()) metalN = n;
        } else if (nameHas(l, "diffuse") || nameHas(l, "albedo") ||
                   nameHas(l, "basecolor") || nameHas(l, "base_color")) {
            if (baseN.empty()) baseN = n;
        }
    }
    // slot fallbacks when filename sniffing came up empty.
    if (roughN.empty()) roughN = m.roughness_texname;
    if (metalN.empty()) metalN = m.metallic_texname;
    if (normN.empty())  normN  = !m.normal_texname.empty() ? m.normal_texname
                                                           : m.bump_texname;

    auto pbr = std::make_shared<PbrMaterial>();

    bool haveBase = false;
    if (!baseN.empty()) {
        auto t = loadTexture(cache, baseDir, baseN, /*sRGB=*/true);
        if (t) { pbr->setBasecolor(std::static_pointer_cast<Texture>(t)); haveBase = true; }
    }
    // basecolor factor: a tint over the map, or the flat color when no map (Kd
    // is often absent -> 0, so fall back to white/gray rather than black).
    // with a basecolor map, Kd is a tint (0 means "untinted" -> white). without
    // a map, the flat Kd IS the color and we trust it even when it's black
    // (e.g. black drum shells authored as Kd 0 0 0).
    Color kd = toColor(m.diffuse);
    pbr->setBasecolorFactor(haveBase ? (anyPositive(kd) ? kd : Color(1.0)) : kd);

    if (!metalN.empty()) {
        auto t = loadTexture(cache, baseDir, metalN, /*sRGB=*/false);
        pbr->setMetallicFactor(t ? 1.0 : 0.0);
        if (t) pbr->setMetallic(t);
    } else {
        pbr->setMetallicFactor(0.0);  // dielectric unless a metallic map says otherwise
    }

    if (!roughN.empty()) {
        auto t = loadTexture(cache, baseDir, roughN, /*sRGB=*/false);
        if (t) { pbr->setRoughness(t); pbr->setRoughnessFactor(1.0); }
        else     pbr->setRoughnessFactor(roughnessFromNs(m.shininess));
    } else {
        pbr->setRoughnessFactor(roughnessFromNs(m.shininess));
    }

    if (!normN.empty()) {
        auto t = loadTexture(cache, baseDir, normN, /*sRGB=*/false);
        if (t) pbr->setNormal(t);
    }
    return pbr;
}

// map one wavefront material onto the renderer's material set. the branches are
// ordered by specificity: emissive, then transparent (glass), then textured
// (metallic-roughness PBR), then scalar-metallic, then flat diffuse.
std::shared_ptr<Material> buildMtlMaterial(const tinyobj::material_t& m,
                                           const std::string& baseDir,
                                           TexCache& cache) {
    Color ke = toColor(m.emission);
    if (anyPositive(ke)) {
        return std::make_shared<EmissiveMaterial>(ke);
    }

    bool transparent = m.dissolve < 0.999 ||
                       m.illum == 4 || m.illum == 6 ||
                       m.illum == 7 || m.illum == 9;
    if (transparent) {
        double ior = (m.ior > 1.0) ? m.ior : 1.5;
        Color tint = toColor(m.transmittance);
        if (!anyPositive(tint)) tint = Color(1.0);
        return std::make_shared<DielectricMaterial>(1.0, ior, Color(1.0), tint);
    }

    // scalar-PBR metals (Pm authored directly, no maps) -> conductor.
    if (m.metallic > 0.5) {
        Color kd = toColor(m.diffuse);
        Color f0 = anyPositive(kd) ? kd : toColor(m.specular);
        if (!anyPositive(f0)) f0 = Color(0.9);
        double rough = (m.roughness > 0.0) ? m.roughness
                     : (m.shininess > 0.0 ? std::sqrt(2.0 / (m.shininess + 2.0))
                                          : 0.2);
        rough = std::max(0.02, std::min(1.0, rough));
        return std::make_shared<ConductorMaterial>(f0, rough);
    }

    // everything else -> metallic-roughness PBR, textured OR flat. this honors
    // any roughness/metallic/normal maps and, for untextured entries, yields a
    // dielectric with the flat Kd (a glossy dark surface when Kd is near-black,
    // rather than a wrongly-greyed one).
    return buildPbrMaterial(m, baseDir, cache);
}

// build a material per .mtl entry, index-aligned with material_ids. an entry
// that fails to build falls back to `fallback` rather than dropping geometry.
std::vector<std::shared_ptr<Material>> buildMaterialTable(
        const MeshData& mesh,
        const std::shared_ptr<Material>& fallback) {
    TexCache cache;
    std::vector<std::shared_ptr<Material>> table(mesh.materials.size());
    for (size_t i = 0; i < mesh.materials.size(); ++i) {
        auto mat = buildMtlMaterial(mesh.materials[i], mesh.baseDir, cache);
        table[i] = mat ? mat : fallback;
    }
    return table;
}

// turn a parsed mesh into Triangle primitives in the scene. when `matTable` is
// non-empty, each face uses the material its material_id points at; otherwise
// (or for faces with no material) `fallback` is used.
void emitTriangles(const MeshData& mesh,
                   Scene& scene,
                   const std::vector<std::shared_ptr<Material>>& matTable,
                   const std::shared_ptr<Material>& fallback,
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

            // pick this face's material from the .mtl table when available,
            // else the fallback.
            const std::shared_ptr<Material>* material = &fallback;
            if (!matTable.empty() && f < shape.mesh.material_ids.size()) {
                int mid = shape.mesh.material_ids[f];
                if (mid >= 0 && static_cast<size_t>(mid) < matTable.size() &&
                    matTable[mid]) {
                    material = &matTable[mid];
                }
            }

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
                        uvAt(v0), uvAt(v1), uvAt(v2), *material));
                } else {
                    scene.addPrimitive(std::make_shared<Triangle>(
                        p0, p1, p2, n0, n1, n2, *material));
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
                     const glm::dmat4& transform,
                     bool preferFileMaterials) const {
    MeshData mesh;
    if (!parseObj(path, mesh)) return false;
    std::vector<std::shared_ptr<Material>> matTable;
    if (preferFileMaterials && !mesh.materials.empty()) {
        matTable = buildMaterialTable(mesh, fallbackMaterial);
    }
    emitTriangles(mesh, scene, matTable, fallbackMaterial, transform);
    return true;
}

bool ObjLoader::loadFitted(const std::string& path,
                           Scene& scene,
                           std::shared_ptr<Material> fallbackMaterial,
                           const glm::dvec3& center,
                           double targetSize,
                           bool sitOnGround,
                           bool preferFileMaterials) const {
    MeshData mesh;
    if (!parseObj(path, mesh)) return false;
    std::vector<std::shared_ptr<Material>> matTable;
    if (preferFileMaterials && !mesh.materials.empty()) {
        matTable = buildMaterialTable(mesh, fallbackMaterial);
    }
    glm::dmat4 M = computeFitTransform(mesh, center, targetSize, sitOnGround);
    emitTriangles(mesh, scene, matTable, fallbackMaterial, M);
    return true;
}
