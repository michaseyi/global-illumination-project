// glTF 2.0 loader, tinygltf-backed.
//
// glTF shares our engine's coordinate frame (right-handed, Y-up, -Z forward),
// so positions/normals import without any axis swap. what it gives us beyond
// OBJ: real metallic-roughness materials (no Ns guessing), punctual lights, and
// a camera - a whole scene from one file.
//
// supported subset: TRIANGLES primitives with POSITION/NORMAL/TEXCOORD_0;
// metallic-roughness materials with base-color / metallic-roughness / normal /
// emissive textures (+ KHR_materials_emissive_strength); the node hierarchy's
// TRS/matrix transforms; the first perspective camera; and KHR_lights_punctual
// point/spot/directional lights. morph targets, skins, and animation are
// ignored.

#include "io/gltf_loader.h"

#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

// stb_image's implementation already lives in image_texture.cpp; tell tinygltf
// to use those symbols rather than emit a second copy, and skip the writer.
#include "stb_image.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "geometry/geometry.h"
#include "geometry/triangle.h"
#include "scene/scene.h"
#include "shading/shading.h"
#include "shading/image_texture.h"
#include "shading/pbr_material.h"

namespace {

using tinygltf::Model;

bool endsWithLower(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    for (size_t i = 0; i < suf.size(); ++i) {
        if (std::tolower((unsigned char)s[s.size() - suf.size() + i]) != suf[i])
            return false;
    }
    return true;
}

// ---- node transforms -------------------------------------------------------

glm::dmat4 localMatrix(const tinygltf::Node& n) {
    if (n.matrix.size() == 16) {
        return glm::make_mat4(n.matrix.data());  // glTF matrices are column-major
    }
    glm::dmat4 m(1.0);
    if (n.translation.size() == 3) {
        m = glm::translate(m, glm::dvec3(n.translation[0], n.translation[1],
                                         n.translation[2]));
    }
    if (n.rotation.size() == 4) {
        glm::dquat q(n.rotation[3], n.rotation[0], n.rotation[1], n.rotation[2]);
        m = m * glm::dmat4(glm::mat4_cast(q));
    }
    if (n.scale.size() == 3) {
        m = glm::scale(m, glm::dvec3(n.scale[0], n.scale[1], n.scale[2]));
    }
    return m;
}

// ---- accessor reading ------------------------------------------------------

const unsigned char* accessorBase(const Model& model,
                                  const tinygltf::Accessor& acc, int& stride) {
    const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer& buf = model.buffers[view.buffer];
    stride = acc.ByteStride(view);
    return buf.data.data() + view.byteOffset + acc.byteOffset;
}

std::vector<glm::dvec3> readVec3(const Model& model, int accessorIndex) {
    std::vector<glm::dvec3> out;
    if (accessorIndex < 0) return out;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    int stride = 0;
    const unsigned char* base = accessorBase(model, acc, stride);
    out.reserve(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        const float* f = reinterpret_cast<const float*>(base + i * stride);
        out.emplace_back(f[0], f[1], f[2]);
    }
    return out;
}

std::vector<glm::dvec2> readVec2(const Model& model, int accessorIndex) {
    std::vector<glm::dvec2> out;
    if (accessorIndex < 0) return out;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    int stride = 0;
    const unsigned char* base = accessorBase(model, acc, stride);
    out.reserve(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        const float* f = reinterpret_cast<const float*>(base + i * stride);
        out.emplace_back(f[0], f[1]);
    }
    return out;
}

std::vector<uint32_t> readIndices(const Model& model, int accessorIndex) {
    std::vector<uint32_t> out;
    if (accessorIndex < 0) return out;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    int stride = 0;
    const unsigned char* base = accessorBase(model, acc, stride);
    out.reserve(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        const unsigned char* p = base + i * stride;
        switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                out.push_back(*reinterpret_cast<const uint32_t*>(p)); break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                out.push_back(*reinterpret_cast<const uint16_t*>(p)); break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                out.push_back(*p); break;
            default: break;
        }
    }
    return out;
}

// ---- textures --------------------------------------------------------------

// texture builder with a cache keyed by (glTF texture index, role). the role
// matters because base-color/emissive decode as sRGB while data maps (normal,
// metallic-roughness) stay linear, and metallic-roughness is split per channel.
struct TextureBuilder {
    const Model& model;
    std::map<std::string, std::shared_ptr<ImageTexture>> cache;

    const tinygltf::Image* imageFor(int textureIndex) {
        if (textureIndex < 0 || textureIndex >= (int)model.textures.size())
            return nullptr;
        int src = model.textures[textureIndex].source;
        if (src < 0 || src >= (int)model.images.size()) return nullptr;
        const tinygltf::Image& img = model.images[src];
        if (img.image.empty()) {
            std::cerr << "[gltf] texture " << textureIndex << " (image \""
                      << img.name << "\") has no decoded pixels\n";
            return nullptr;
        }
        return &img;
    }

    std::shared_ptr<ImageTexture> color(int textureIndex, bool sRGB) {
        const tinygltf::Image* img = imageFor(textureIndex);
        if (!img) return nullptr;
        std::string key = "c" + std::to_string(textureIndex) + (sRGB ? "s" : "l");
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        auto t = ImageTexture::fromPixels(img->image.data(), img->width,
                                          img->height, img->component, sRGB);
        cache[key] = t;
        return t;
    }

    // extract one channel of a texture as a single-channel linear map (used to
    // split glTF's packed metallic-roughness: roughness=G, metallic=B).
    std::shared_ptr<ImageTexture> channel(int textureIndex, int ch) {
        const tinygltf::Image* img = imageFor(textureIndex);
        if (!img || ch >= img->component) return nullptr;
        std::string key = "ch" + std::to_string(textureIndex) + "_" + std::to_string(ch);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        std::vector<unsigned char> single(size_t(img->width) * img->height);
        for (size_t i = 0; i < single.size(); ++i) {
            single[i] = img->image[i * img->component + ch];
        }
        auto t = ImageTexture::fromPixels(single.data(), img->width, img->height, 1,
                                          /*sRGB=*/false);
        cache[key] = t;
        return t;
    }
};

// which TEXCOORD_n set this material's textures reference. glTF allows a texture
// to use a second UV set (e.g. Blender's plaster/concrete here use set 1); we
// pick one set per material (they're consistent in practice) and feed it as the
// mesh UVs. without this, textures sample the wrong UVs -> mapping artifacts.
int materialTexCoord(const tinygltf::Material& m) {
    const auto& bc = m.pbrMetallicRoughness.baseColorTexture;
    if (bc.index >= 0) return bc.texCoord;
    if (m.normalTexture.index >= 0) return m.normalTexture.texCoord;
    return 0;
}

// KHR_texture_transform: UV offset/rotation/scale for tiling. read from the
// base-color texture (materials here use one transform for all their maps).
struct UvXform {
    glm::dvec2 offset{0.0, 0.0};
    glm::dvec2 scale{1.0, 1.0};
    double rot = 0.0;
    bool active = false;
};

UvXform materialUvXform(const tinygltf::Material& m) {
    UvXform x;
    const auto& bc = m.pbrMetallicRoughness.baseColorTexture;
    auto it = bc.extensions.find("KHR_texture_transform");
    if (it == bc.extensions.end()) return x;
    const tinygltf::Value& e = it->second;
    if (e.Has("offset")) {
        const auto& o = e.Get("offset");
        if (o.IsArray() && o.ArrayLen() >= 2) {
            x.offset.x = o.Get(0).GetNumberAsDouble();
            x.offset.y = o.Get(1).GetNumberAsDouble();
        }
    }
    if (e.Has("scale")) {
        const auto& s = e.Get("scale");
        if (s.IsArray() && s.ArrayLen() >= 2) {
            x.scale.x = s.Get(0).GetNumberAsDouble();
            x.scale.y = s.Get(1).GetNumberAsDouble();
        }
    }
    if (e.Has("rotation")) x.rot = e.Get("rotation").GetNumberAsDouble();
    x.active = true;
    return x;
}

double emissiveStrength(const tinygltf::Material& m) {
    auto it = m.extensions.find("KHR_materials_emissive_strength");
    if (it != m.extensions.end() && it->second.Has("emissiveStrength")) {
        return it->second.Get("emissiveStrength").GetNumberAsDouble();
    }
    return 1.0;
}

// read a scalar from a material extension, or `def` if absent.
double extScalar(const tinygltf::Material& m, const char* ext, const char* key,
                 double def) {
    auto it = m.extensions.find(ext);
    if (it != m.extensions.end() && it->second.Has(key)) {
        return it->second.Get(key).GetNumberAsDouble();
    }
    return def;
}

std::shared_ptr<Material> buildMaterial(const Model& model,
                                        const tinygltf::Material& m,
                                        TextureBuilder& tb) {
    const auto& pbr = m.pbrMetallicRoughness;

    // emissive materials become light sources. Blender exports fixture emission
    // via emissiveFactor (0..1) scaled by KHR_materials_emissive_strength.
    glm::dvec3 ke(m.emissiveFactor.size() == 3
                      ? glm::dvec3(m.emissiveFactor[0], m.emissiveFactor[1],
                                   m.emissiveFactor[2])
                      : glm::dvec3(0.0));
    ke *= emissiveStrength(m);
    if (ke.r + ke.g + ke.b > 1e-3) {
        return std::make_shared<EmissiveMaterial>(Color(ke.r, ke.g, ke.b));
    }

    Color baseFactor(pbr.baseColorFactor.size() == 4 ? pbr.baseColorFactor[0] : 1.0,
                     pbr.baseColorFactor.size() == 4 ? pbr.baseColorFactor[1] : 1.0,
                     pbr.baseColorFactor.size() == 4 ? pbr.baseColorFactor[2] : 1.0);

    // transmissive materials (glass/windows) -> dielectric, so light passes
    // through. without this a glass window is an opaque wall that seals the room.
    if (extScalar(m, "KHR_materials_transmission", "transmissionFactor", 0.0) > 0.5) {
        double ior = extScalar(m, "KHR_materials_ior", "ior", 1.5);
        return std::make_shared<DielectricMaterial>(
            1.0, ior > 1.0 ? ior : 1.5, Color(1.0), baseFactor);
    }

    auto mat = std::make_shared<PbrMaterial>();
    mat->setBasecolorFactor(baseFactor);
    if (auto t = tb.color(pbr.baseColorTexture.index, /*sRGB=*/true)) {
        mat->setBasecolor(std::static_pointer_cast<Texture>(t));
    }

    mat->setMetallicFactor(pbr.metallicFactor);
    mat->setRoughnessFactor(pbr.roughnessFactor);
    if (pbr.metallicRoughnessTexture.index >= 0) {
        // glTF packs roughness in G and metallic in B of one image.
        if (auto r = tb.channel(pbr.metallicRoughnessTexture.index, 1))
            mat->setRoughness(r);
        if (auto met = tb.channel(pbr.metallicRoughnessTexture.index, 2))
            mat->setMetallic(met);
    }
    if (m.normalTexture.index >= 0) {
        if (auto n = tb.color(m.normalTexture.index, /*sRGB=*/false))
            mat->setNormal(n);
    }

    // procedural mirror/chrome shaders can't be expressed in glTF's metallic-
    // roughness model, so Blender exports them as a fully-rough metal (no
    // reflection). recognize them by name and restore a smooth mirror finish.
    std::string lname = m.name;
    for (auto& c : lname) c = char(std::tolower((unsigned char)c));
    if (lname.find("mirror") != std::string::npos ||
        lname.find("chrome") != std::string::npos) {
        mat->setMetallicFactor(1.0);
        mat->setRoughnessFactor(0.02);
        std::cerr << "[gltf] \"" << m.name << "\" treated as a mirror "
                     "(procedural material flattened by glTF export)\n";
    }
    return mat;
}

// ---- lights ----------------------------------------------------------------

void addLight(Scene& scene, const tinygltf::Light& l, const glm::dmat4& world,
              double lightScale) {
    Color color(l.color.size() == 3 ? l.color[0] : 1.0,
                l.color.size() == 3 ? l.color[1] : 1.0,
                l.color.size() == 3 ? l.color[2] : 1.0);
    glm::dvec3 pos(world[3]);
    glm::dvec3 dir = glm::normalize(glm::dvec3(-world[2]));  // local -Z travel dir
    double intensity = l.intensity * lightScale;

    if (l.type == "directional") {
        // glTF directional intensity is lux; our units are arbitrary, so
        // lightScale carries the conversion. a true parallel-ray sun.
        scene.addLight(std::make_shared<DirectionalLight>(
            dir, Color(color.r * intensity, color.g * intensity,
                       color.b * intensity)));
        return;
    }
    // point and spot (cone ignored) both emit from their position.
    scene.addLight(std::make_shared<PointLight>(pos, color, intensity));
}

// ---- mesh emission ---------------------------------------------------------

void emitPrimitive(Scene& scene, const Model& model,
                   const tinygltf::Primitive& prim,
                   const std::shared_ptr<Material>& material,
                   const glm::dmat4& world, int texCoordSet,
                   const UvXform& xform,
                   glm::dvec3& bbMin, glm::dvec3& bbMax) {
    if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1) return;

    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) return;
    std::vector<glm::dvec3> pos = readVec3(model, posIt->second);
    if (pos.empty()) return;

    auto nrmIt = prim.attributes.find("NORMAL");
    std::vector<glm::dvec3> nrm =
        (nrmIt != prim.attributes.end()) ? readVec3(model, nrmIt->second)
                                         : std::vector<glm::dvec3>();
    // read the UV set the material references, falling back to set 0.
    auto uvIt = prim.attributes.find("TEXCOORD_" + std::to_string(texCoordSet));
    if (uvIt == prim.attributes.end()) uvIt = prim.attributes.find("TEXCOORD_0");
    std::vector<glm::dvec2> uv =
        (uvIt != prim.attributes.end()) ? readVec2(model, uvIt->second)
                                        : std::vector<glm::dvec2>();
    if (xform.active) {  // apply KHR_texture_transform (offset/rotation/scale)
        double c = std::cos(xform.rot), s = std::sin(xform.rot);
        for (auto& t : uv) {
            double su = t.x * xform.scale.x, sv = t.y * xform.scale.y;
            t = glm::dvec2(c * su - s * sv + xform.offset.x,
                           s * su + c * sv + xform.offset.y);
        }
    }

    glm::dmat3 normalMatrix = glm::dmat3(glm::transpose(glm::inverse(world)));
    auto P = [&](uint32_t i) { return glm::dvec3(world * glm::dvec4(pos[i], 1.0)); };
    auto N = [&](uint32_t i) {
        return nrm.empty() ? glm::dvec3(0.0)
                           : glm::normalize(normalMatrix * nrm[i]);
    };

    std::vector<uint32_t> idx = readIndices(model, prim.indices);
    if (idx.empty()) {  // non-indexed: sequential triples
        idx.resize(pos.size());
        for (uint32_t i = 0; i < pos.size(); ++i) idx[i] = i;
    }

    const bool hasUV = !uv.empty();
    const bool hasN  = !nrm.empty();
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        uint32_t a = idx[t], b = idx[t + 1], c = idx[t + 2];
        if (a >= pos.size() || b >= pos.size() || c >= pos.size()) continue;
        glm::dvec3 p0 = P(a), p1 = P(b), p2 = P(c);
        bbMin = glm::min(bbMin, glm::min(p0, glm::min(p1, p2)));
        bbMax = glm::max(bbMax, glm::max(p0, glm::max(p1, p2)));
        glm::dvec3 n0, n1, n2;
        if (hasN) { n0 = N(a); n1 = N(b); n2 = N(c); }
        else {
            glm::dvec3 fn = glm::normalize(glm::cross(p1 - p0, p2 - p0));
            n0 = n1 = n2 = fn;
        }
        if (hasUV) {
            scene.addPrimitive(std::make_shared<Triangle>(
                p0, p1, p2, n0, n1, n2, uv[a], uv[b], uv[c], material));
        } else {
            scene.addPrimitive(std::make_shared<Triangle>(
                p0, p1, p2, n0, n1, n2, material));
        }
    }
}

// ---- traversal -------------------------------------------------------------

struct FoundCamera {
    int cameraIndex;      // index into model.cameras
    glm::dmat4 world;
    std::string nodeName;
};

struct Traversal {
    const Model& model;
    Scene& scene;
    std::vector<std::shared_ptr<Material>>& materials;
    const std::vector<int>& matTexCoord;
    const std::vector<UvXform>& matUvXform;
    TextureBuilder& tb;
    double lightScale;
    std::shared_ptr<Material> fallback;

    std::vector<FoundCamera> cameras;
    glm::dvec3 bbMin{1e18};
    glm::dvec3 bbMax{-1e18};

    void visit(int nodeIndex, const glm::dmat4& parent) {
        const tinygltf::Node& node = model.nodes[nodeIndex];
        glm::dmat4 world = parent * localMatrix(node);

        if (node.mesh >= 0) {
            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (const auto& prim : mesh.primitives) {
                bool valid = prim.material >= 0 &&
                             prim.material < (int)materials.size();
                std::shared_ptr<Material> mat = valid ? materials[prim.material]
                                                      : fallback;
                int tc = valid ? matTexCoord[prim.material] : 0;
                static const UvXform kNoXform;
                const UvXform& xf = valid ? matUvXform[prim.material] : kNoXform;
                emitPrimitive(scene, model, prim, mat, world, tc, xf, bbMin, bbMax);
            }
        }
        if (node.light >= 0 && node.light < (int)model.lights.size()) {
            addLight(scene, model.lights[node.light], world, lightScale);
        }
        if (node.camera >= 0) {
            cameras.push_back({node.camera, world, node.name});
        }
        for (int child : node.children) visit(child, world);
    }
};

// pick the camera whose node name contains `select` (case-insensitive), or the
// one at index `select` if it's all digits, else the first. -1 index if none.
int selectCamera(const std::vector<FoundCamera>& cams, const std::string& select) {
    if (cams.empty()) return -1;
    if (select.empty()) return 0;
    bool allDigits = true;
    for (char c : select) if (!std::isdigit((unsigned char)c)) allDigits = false;
    if (allDigits) {
        int idx = std::atoi(select.c_str());
        return (idx >= 0 && idx < (int)cams.size()) ? idx : 0;
    }
    std::string want;
    for (char c : select) want += char(std::tolower((unsigned char)c));
    for (size_t i = 0; i < cams.size(); ++i) {
        std::string nm;
        for (char c : cams[i].nodeName) nm += char(std::tolower((unsigned char)c));
        if (nm.find(want) != std::string::npos) return int(i);
    }
    return 0;
}

}  // namespace

std::unique_ptr<SceneSetup> GltfLoader::loadFromFile(const std::string& path,
                                                     int width, int height,
                                                     double lightScale,
                                                     const std::string& cameraSelect,
                                                     double ceilingLight) {
    Model model;
    tinygltf::TinyGLTF ctx;
    std::string err, warn;
    bool ok = endsWithLower(path, ".glb")
                  ? ctx.LoadBinaryFromFile(&model, &err, &warn, path)
                  : ctx.LoadASCIIFromFile(&model, &err, &warn, path);
    if (!warn.empty()) std::cerr << "[gltf warn] " << warn << "\n";
    if (!err.empty())  std::cerr << "[gltf err] "  << err << "\n";
    if (!ok) { std::cerr << "[gltf] failed to load " << path << "\n"; return nullptr; }

    int decoded = 0, emptyImg = 0;
    for (const auto& im : model.images) (im.image.empty() ? emptyImg : decoded)++;
    std::cerr << "[gltf] images: " << decoded << " decoded, " << emptyImg
              << " empty (of " << model.images.size() << ")\n";

    Scene scene;
    TextureBuilder tb{model, {}};

    std::vector<std::shared_ptr<Material>> materials;
    std::vector<int> matTexCoord;
    std::vector<UvXform> matUvXform;
    materials.reserve(model.materials.size());
    matTexCoord.reserve(model.materials.size());
    matUvXform.reserve(model.materials.size());
    for (const auto& m : model.materials) {
        materials.push_back(buildMaterial(model, m, tb));
        matTexCoord.push_back(materialTexCoord(m));
        matUvXform.push_back(materialUvXform(m));
    }
    auto fallback = std::make_shared<LambertMaterial>(Color(0.72, 0.72, 0.72));

    Traversal trav{model, scene, materials, matTexCoord, matUvXform, tb,
                   lightScale, fallback, {}};
    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (model.scenes.empty()) {
        std::cerr << "[gltf] no scenes in file\n";
        return nullptr;
    }
    for (int root : model.scenes[sceneIndex].nodes) trav.visit(root, glm::dmat4(1.0));

    // optional ceiling area light spanning the room's bounding box, facing down.
    if (ceilingLight > 0.0 && trav.bbMax.y > trav.bbMin.y) {
        glm::dvec3 lo = trav.bbMin, hi = trav.bbMax;
        std::cerr << "[gltf] scene bounds min(" << lo.x << "," << lo.y << "," << lo.z
                  << ") max(" << hi.x << "," << hi.y << "," << hi.z << ")\n";
        double mx = (hi.x - lo.x) * 0.15, mz = (hi.z - lo.z) * 0.15;
        // drop it well below the ceiling slab so it hangs inside the room.
        double y = hi.y - std::max(0.4, (hi.y - lo.y) * 0.12);
        glm::dvec3 origin(lo.x + mx, y, lo.z + mz);
        glm::dvec3 u(hi.x - lo.x - 2 * mx, 0, 0);   // cross(u,v) points -y (down)
        glm::dvec3 v(0, 0, hi.z - lo.z - 2 * mz);
        Color emission(ceilingLight, ceilingLight * 0.96, ceilingLight * 0.9);
        auto quad = std::make_shared<Quad>(
            origin, u, v, std::make_shared<EmissiveMaterial>(emission));
        auto light = std::make_shared<AreaLight>(quad, emission, false);
        scene.addAreaLight(quad, light);
        std::cerr << "[gltf] added ceiling area light at y=" << y
                  << " emission=" << ceilingLight << "\n";
    }

    scene.build();

    // camera: pick the requested one (by name/index), else the first.
    glm::dvec3 eye(0, 1, 3), target(0, 1, 0), up(0, 1, 0);
    double fovDeg = 45.0;
    int pick = selectCamera(trav.cameras, cameraSelect);
    if (pick >= 0) {
        const FoundCamera& fc = trav.cameras[pick];
        const tinygltf::Camera& cam = model.cameras[fc.cameraIndex];
        eye    = glm::dvec3(fc.world[3]);
        target = eye + glm::normalize(glm::dvec3(-fc.world[2]));
        up     = glm::normalize(glm::dvec3(fc.world[1]));
        if (cam.type == "perspective" && cam.perspective.yfov > 0.0) {
            fovDeg = cam.perspective.yfov * 180.0 / 3.14159265358979323846;
        }
        std::cerr << "[gltf] using camera \"" << fc.nodeName << "\" ("
                  << (pick + 1) << "/" << trav.cameras.size() << ")\n";
    } else {
        std::cerr << "[gltf] no camera in file - using a default view\n";
    }
    Camera camera(eye, target, up, fovDeg, width, height);

    std::cerr << "[gltf] loaded " << model.meshes.size() << " meshes, "
              << model.materials.size() << " materials, "
              << model.lights.size() << " lights, "
              << trav.cameras.size() << " cameras\n";

    return std::unique_ptr<SceneSetup>(
        new SceneSetup{std::move(scene), camera});
}
