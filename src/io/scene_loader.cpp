// json scene loader. walks a parsed json tree (see io/json.h) and constructs
// materials, primitives, lights and a camera into a SceneSetup.
//
// schema (all vectors are json arrays; unknown keys are ignored):
//   {
//     "camera":   { "eye":[x,y,z], "target":[x,y,z], "up":[x,y,z],
//                   "fov":deg, "aperture":a, "focus":d },
//     "materials": {
//       "<name>": { "type":"lambert",    "color":[r,g,b] | "texture":"file" }
//                | { "type":"mirror",    "color":[r,g,b] }
//                | { "type":"dielectric","ior":n, "tint":[r,g,b] }
//                | { "type":"conductor", "color":[r,g,b], "roughness":r }
//                | { "type":"emissive",  "color":[r,g,b] }
//     },
//     "objects": [
//       { "type":"sphere", "center":[..], "radius":r, "material":"name" },
//       { "type":"quad",   "origin":[..], "u":[..], "v":[..], "material":"name" },
//       { "type":"box",    "center":[..], "size":[..], "yaw":deg, "material":"name" },
//       { "type":"mesh",   "file":"model.obj", "material":"name",
//         "fit":{ "center":[..], "size":s, "sitOnGround":true },
//         "useFileMaterials":false }
//     ],
//     "lights": [
//       { "type":"area",  "origin":[..], "u":[..], "v":[..],
//         "color":[r,g,b], "twoSided":false },
//       { "type":"point", "position":[..], "color":[r,g,b], "intensity":i }
//     ]
//   }
//
// paths (textures, meshes) resolve relative to the scene file's directory.

#include "io/scene_loader.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "core/constants.h"
#include "geometry/geometry.h"
#include "io/json.h"
#include "io/obj_loader.h"
#include "shading/image_texture.h"
#include "shading/shading.h"

namespace {

std::string dirOf(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? std::string() : path.substr(0, s + 1);
}

std::string resolve(const std::string& baseDir, const std::string& rel) {
    if (rel.empty() || rel[0] == '/') return rel;
    return baseDir + rel;
}

glm::dvec3 vec3(const json::Value& v, const glm::dvec3& fallback) {
    if (!v.isArray() || v.size() < 3) return fallback;
    return glm::dvec3(v[size_t(0)].asNumber(fallback.x),
                      v[size_t(1)].asNumber(fallback.y),
                      v[size_t(2)].asNumber(fallback.z));
}

Color color(const json::Value& v, const Color& fallback) {
    glm::dvec3 c = vec3(v, glm::dvec3(fallback.r, fallback.g, fallback.b));
    return Color(c.x, c.y, c.z);
}

// 12-triangle axis-aligned box with a yaw rotation (mirrors scene_factory).
void addBox(Scene& scene, const glm::dvec3& center, const glm::dvec3& size,
            double yawDeg, const std::shared_ptr<Material>& mat) {
    double yaw = yawDeg * constants::kPi / 180.0;
    double cs = std::cos(yaw), sn = std::sin(yaw);
    auto rot = [&](glm::dvec3 v) {
        return glm::dvec3(cs * v.x + sn * v.z, v.y, -sn * v.x + cs * v.z);
    };
    glm::dvec3 h = size * 0.5;
    glm::dvec3 c[8];
    for (int i = 0; i < 8; ++i) {
        glm::dvec3 lc((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
        c[i] = center + rot(lc);
    }
    int faces[6][4] = {
        {0,1,5,4}, {2,6,7,3}, {0,4,6,2}, {1,3,7,5}, {0,2,3,1}, {4,5,7,6}
    };
    for (auto& q : faces) {
        scene.addPrimitive(std::make_shared<Triangle>(c[q[0]], c[q[1]], c[q[2]], mat));
        scene.addPrimitive(std::make_shared<Triangle>(c[q[0]], c[q[2]], c[q[3]], mat));
    }
}

std::shared_ptr<Material> buildMaterial(const json::Value& m,
                                        const std::string& baseDir) {
    std::string type = m["type"].asString("lambert");
    if (type == "mirror") {
        return std::make_shared<MirrorMaterial>(color(m["color"], Color(0.95)));
    }
    if (type == "dielectric") {
        double ior = m["ior"].asNumber(1.5);
        Color tint = m.has("tint") ? color(m["tint"], Color(1.0)) : Color(1.0);
        return std::make_shared<DielectricMaterial>(1.0, ior, Color(1.0), tint);
    }
    if (type == "conductor") {
        return std::make_shared<ConductorMaterial>(
            color(m["color"], Color(0.9, 0.9, 0.9)),
            m["roughness"].asNumber(0.1));
    }
    if (type == "emissive") {
        return std::make_shared<EmissiveMaterial>(color(m["color"], Color(1.0)));
    }
    // lambert (default): texture takes precedence over flat color.
    if (m.has("texture")) {
        auto tex = ImageTexture::load(resolve(baseDir, m["texture"].asString()),
                                      /*sRGB=*/true, ImageTexture::Wrap::Repeat);
        if (tex) return std::make_shared<LambertMaterial>(
                     std::static_pointer_cast<Texture>(tex));
        std::cerr << "[scene] texture load failed: " << m["texture"].asString()
                  << " — using flat color\n";
    }
    return std::make_shared<LambertMaterial>(color(m["color"], Color(0.73)));
}

}  // namespace

std::unique_ptr<SceneSetup> SceneLoader::loadFromFile(const std::string& path,
                                                     int width, int height) {
    std::ifstream in(path);
    if (!in) { std::cerr << "[scene] cannot open " << path << "\n"; return nullptr; }
    std::stringstream ss;
    ss << in.rdbuf();

    std::string err;
    json::Value root = json::parse(ss.str(), err);
    if (!err.empty() || !root.isObject()) {
        std::cerr << "[scene] parse error in " << path << ": "
                  << (err.empty() ? "root is not an object" : err) << "\n";
        return nullptr;
    }

    const std::string baseDir = dirOf(path);
    Scene scene;

    // materials
    std::map<std::string, std::shared_ptr<Material>> materials;
    const json::Value& mats = root["materials"];
    if (mats.isObject()) {
        for (const auto& kv : mats.obj) {
            materials[kv.first] = buildMaterial(kv.second, baseDir);
        }
    }
    auto lookup = [&](const std::string& name) -> std::shared_ptr<Material> {
        auto it = materials.find(name);
        if (it != materials.end()) return it->second;
        if (!name.empty()) {
            std::cerr << "[scene] unknown material \"" << name
                      << "\" — using gray\n";
        }
        return std::make_shared<LambertMaterial>(Color(0.73));
    };

    // objects
    const json::Value& objects = root["objects"];
    for (size_t i = 0; i < objects.size(); ++i) {
        const json::Value& o = objects[i];
        std::string type = o["type"].asString();
        auto mat = lookup(o["material"].asString());

        if (type == "sphere") {
            scene.addPrimitive(std::make_shared<Sphere>(
                vec3(o["center"], glm::dvec3(0.0)),
                o["radius"].asNumber(1.0), mat));
        } else if (type == "quad") {
            scene.addPrimitive(std::make_shared<Quad>(
                vec3(o["origin"], glm::dvec3(0.0)),
                vec3(o["u"], glm::dvec3(1, 0, 0)),
                vec3(o["v"], glm::dvec3(0, 0, 1)), mat));
        } else if (type == "box") {
            addBox(scene, vec3(o["center"], glm::dvec3(0.0)),
                   vec3(o["size"], glm::dvec3(1.0)),
                   o["yaw"].asNumber(0.0), mat);
        } else if (type == "mesh") {
            ObjLoader loader;
            std::string file = resolve(baseDir, o["file"].asString());
            bool useFileMats = o["useFileMaterials"].asBool(false);
            const json::Value& fit = o["fit"];
            bool ok;
            if (fit.isObject()) {
                ok = loader.loadFitted(file, scene, mat,
                                       vec3(fit["center"], glm::dvec3(0.0)),
                                       fit["size"].asNumber(1.0),
                                       fit["sitOnGround"].asBool(true),
                                       useFileMats);
            } else {
                ok = loader.load(file, scene, mat, glm::dmat4(1.0), useFileMats);
            }
            if (!ok) std::cerr << "[scene] mesh load failed: " << file << "\n";
        } else {
            std::cerr << "[scene] unknown object type \"" << type << "\"\n";
        }
    }

    // lights
    const json::Value& lights = root["lights"];
    for (size_t i = 0; i < lights.size(); ++i) {
        const json::Value& l = lights[i];
        std::string type = l["type"].asString();
        if (type == "area") {
            Color emission = color(l["color"], Color(10.0));
            bool twoSided = l["twoSided"].asBool(false);
            auto quad = std::make_shared<Quad>(
                vec3(l["origin"], glm::dvec3(0.0)),
                vec3(l["u"], glm::dvec3(1, 0, 0)),
                vec3(l["v"], glm::dvec3(0, 0, 1)),
                std::make_shared<EmissiveMaterial>(emission));
            auto light = std::make_shared<AreaLight>(quad, emission, twoSided);
            scene.addAreaLight(quad, light);
        } else if (type == "point") {
            scene.addLight(std::make_shared<PointLight>(
                vec3(l["position"], glm::dvec3(0.0)),
                color(l["color"], Color(1.0)),
                l["intensity"].asNumber(10.0)));
        } else {
            std::cerr << "[scene] unknown light type \"" << type << "\"\n";
        }
    }

    scene.build();

    // camera
    const json::Value& cam = root["camera"];
    Camera camera(vec3(cam["eye"], glm::dvec3(0, 1, 3)),
                  vec3(cam["target"], glm::dvec3(0, 1, 0)),
                  vec3(cam["up"], glm::dvec3(0, 1, 0)),
                  cam["fov"].asNumber(40.0), width, height,
                  cam["aperture"].asNumber(0.0),
                  cam["focus"].asNumber(1.0));

    // optional uniform environment ("background"/"environment"): a scalar or
    // [r,g,b]. rays that escape the scene return this radiance, so it doubles as
    // a soft fill/daylight through windows and openings.
    Color bg(0.0);
    const json::Value& env = root.has("background") ? root["background"]
                                                    : root["environment"];
    if (env.isArray())       bg = color(env, Color(0.0));
    else if (env.isNumber()) bg = Color(env.asNumber(0.0));

    return std::unique_ptr<SceneSetup>(
        new SceneSetup{std::move(scene), camera, bg});
}
