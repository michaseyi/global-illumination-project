#include <QApplication>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "app/gui.h"
#include "app/scene_factory.h"
#include "io/image.h"
#include "io/scene_loader.h"
#include "io/gltf_loader.h"
#include "render/direct_lighting_integrator.h"
#include "render/path_tracing_integrator.h"
#include "render/renderer.h"
#include "render/whitted_integrator.h"
#include "shading/pbr_material.h"

namespace {

struct Options {
    std::string scene = "cornell";
    std::string integratorName = "path";
    std::string sphereKind = "glass";
    std::string outPath = "render.png";
    std::string pbrDir;
    std::string objPath;                 // mesh file for --scene mesh
    std::string meshMaterial = "diffuse";
    int width = 600;
    int height = 600;
    int spp = 32;
    int maxDepth = 8;
    int threads = 0;
    bool gui = false;
    bool headless = false;
    bool hasBg = false;      // --bg overrides the scene/default environment
    Color bgColor{0.0};      // uniform environment radiance for escaped rays
    double lightScale = 1.0; // multiplier for glTF punctual-light intensity
    std::string camera;      // glTF camera to use (node-name substring or index)
    double ceilingLight = 0.0; // glTF: add a ceiling area light of this intensity
    std::string camEye;      // glTF: override camera eye "x,y,z"
    std::string camTarget;   // glTF: override camera target "x,y,z"
    double camFov = 0.0;     // glTF: override camera vertical fov (degrees)
    std::string sampler = "stratified";  // sampling strategy: stratified|random
    std::string accel = "bvh";           // ray-query structure: bvh|octree|brute
    std::string envPath;      // equirect .hdr environment (overrides scene's)
    double envScale = 1.0;
    double envRot = 0.0;      // degrees around +Y
};

// parse "0.6" or "0.6,0.7,1.0" into a Color.
Color parseColor(const std::string& s) {
    double r = 0, g = 0, b = 0;
    if (std::sscanf(s.c_str(), "%lf,%lf,%lf", &r, &g, &b) == 3) return Color(r, g, b);
    double v = std::atof(s.c_str());
    return Color(v, v, v);
}

bool nextArg(int argc, char** argv, int& i, const char*& v) {
    if (i + 1 >= argc) return false;
    v = argv[++i];
    return true;
}

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* k = argv[i];
        const char* v = nullptr;
        // each value-taking flag only assigns when a value actually follows,
        // so a trailing flag like `--w` (no value) can't deref a null pointer.
        if      (!std::strcmp(k, "--gui")) o.gui = true;
        else if (!std::strcmp(k, "--headless")) o.headless = true;
        else if (!std::strcmp(k, "--scene"))        { if (nextArg(argc, argv, i, v)) o.scene = v; }
        else if (!std::strcmp(k, "--integrator"))   { if (nextArg(argc, argv, i, v)) o.integratorName = v; }
        else if (!std::strcmp(k, "--sphere"))       { if (nextArg(argc, argv, i, v)) o.sphereKind = v; }
        else if (!std::strcmp(k, "--out"))          { if (nextArg(argc, argv, i, v)) o.outPath = v; }
        else if (!std::strcmp(k, "--pbr-dir"))      { if (nextArg(argc, argv, i, v)) o.pbrDir = v; }
        else if (!std::strcmp(k, "--obj"))          { if (nextArg(argc, argv, i, v)) o.objPath = v; }
        else if (!std::strcmp(k, "--obj-material")) { if (nextArg(argc, argv, i, v)) o.meshMaterial = v; }
        else if (!std::strcmp(k, "--w"))            { if (nextArg(argc, argv, i, v)) o.width = std::atoi(v); }
        else if (!std::strcmp(k, "--h"))            { if (nextArg(argc, argv, i, v)) o.height = std::atoi(v); }
        else if (!std::strcmp(k, "--spp"))          { if (nextArg(argc, argv, i, v)) o.spp = std::atoi(v); }
        else if (!std::strcmp(k, "--max-depth"))    { if (nextArg(argc, argv, i, v)) o.maxDepth = std::atoi(v); }
        else if (!std::strcmp(k, "--threads"))      { if (nextArg(argc, argv, i, v)) o.threads = std::atoi(v); }
        else if (!std::strcmp(k, "--bg"))           { if (nextArg(argc, argv, i, v)) { o.bgColor = parseColor(v); o.hasBg = true; } }
        else if (!std::strcmp(k, "--light-scale"))  { if (nextArg(argc, argv, i, v)) o.lightScale = std::atof(v); }
        else if (!std::strcmp(k, "--camera"))       { if (nextArg(argc, argv, i, v)) o.camera = v; }
        else if (!std::strcmp(k, "--ceiling-light")){ if (nextArg(argc, argv, i, v)) o.ceilingLight = std::atof(v); }
        else if (!std::strcmp(k, "--cam-eye"))      { if (nextArg(argc, argv, i, v)) o.camEye = v; }
        else if (!std::strcmp(k, "--cam-target"))   { if (nextArg(argc, argv, i, v)) o.camTarget = v; }
        else if (!std::strcmp(k, "--cam-fov"))      { if (nextArg(argc, argv, i, v)) o.camFov = std::atof(v); }
        else if (!std::strcmp(k, "--sampler"))      { if (nextArg(argc, argv, i, v)) o.sampler = v; }
        else if (!std::strcmp(k, "--accel"))        { if (nextArg(argc, argv, i, v)) o.accel = v; }
        else if (!std::strcmp(k, "--env"))          { if (nextArg(argc, argv, i, v)) o.envPath = v; }
        else if (!std::strcmp(k, "--env-scale"))    { if (nextArg(argc, argv, i, v)) o.envScale = std::atof(v); }
        else if (!std::strcmp(k, "--env-rot"))      { if (nextArg(argc, argv, i, v)) o.envRot = std::atof(v); }
        else {
            std::cerr << "[warn] unknown arg: " << k << "\n";
        }
    }
    return o;
}

CornellSphere parseSphere(const std::string& s) {
    if (s == "metal")   return CornellSphere::RoughMetal;
    if (s == "diffuse") return CornellSphere::Diffuse;
    return CornellSphere::Glass;
}

MeshMaterial parseMeshMaterial(const std::string& s) {
    if (s == "metal" || s == "gold")         return MeshMaterial::Metal;
    if (s == "glass")                        return MeshMaterial::Glass;
    if (s == "mirror")                       return MeshMaterial::Mirror;
    if (s == "file" || s == "mtl" || s == "auto") return MeshMaterial::FromFile;
    return MeshMaterial::Diffuse;
}

std::shared_ptr<Material> maybeLoadPbr(const std::string& dir);

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// single source of truth for turning Options into a scene, shared by the
// headless and gui paths so they can never drift apart.
SceneSetup buildScene(const Options& o) {
    // a scene name ending in .json is a path to a json scene description.
    if (endsWith(o.scene, ".json")) {
        if (auto setup = SceneLoader::loadFromFile(o.scene, o.width, o.height)) {
            return std::move(*setup);
        }
        std::cerr << "[scene] falling back to cornell box\n";
        return SceneFactory::createCornellBoxScene(o.width, o.height);
    }
    // .glb / .gltf: a full glTF scene (geometry + materials + lights + camera).
    if (endsWith(o.scene, ".glb") || endsWith(o.scene, ".gltf")) {
        if (auto setup = GltfLoader::loadFromFile(o.scene, o.width, o.height,
                                                  o.lightScale, o.camera,
                                                  o.ceilingLight, o.camEye,
                                                  o.camTarget, o.camFov)) {
            return std::move(*setup);
        }
        std::cerr << "[scene] falling back to cornell box\n";
        return SceneFactory::createCornellBoxScene(o.width, o.height);
    }
    if (o.scene == "starter") {
        return SceneFactory::createStarterScene(o.width, o.height);
    }
    if (o.scene == "mesh") {
        std::string path = o.objPath.empty()
            ? std::string(PROJECT_SOURCE_DIR) + "/assets/models/teapot.obj"
            : o.objPath;
        return SceneFactory::createMeshScene(o.width, o.height, path,
                                             parseMeshMaterial(o.meshMaterial));
    }
    if (o.scene == "showcase") {
        return SceneFactory::createGlassShowcaseScene(o.width, o.height);
    }
    auto pbr = maybeLoadPbr(o.pbrDir);
    return SceneFactory::createCornellBoxScene(o.width, o.height,
                                               parseSphere(o.sphereKind), pbr);
}

// resolve the environment for escaped rays: --env wins over the scene file's.
std::shared_ptr<EnvironmentMap> resolveEnv(const Options& o,
                                           const SceneSetup& setup) {
    if (!o.envPath.empty()) {
        return EnvironmentMap::load(o.envPath, o.envScale, o.envRot);
    }
    return setup.envMap;
}

// scene factories build with the default bvh; a non-default --accel triggers a
// timed rebuild here so the choice (and its build cost) is visible.
void applyAccel(Scene& scene, const std::string& name) {
    Scene::AccelType type = Scene::AccelType::BVH;
    if (name == "octree")                    type = Scene::AccelType::Octree;
    else if (name == "brute" || name == "linear") type = Scene::AccelType::Linear;
    else if (name != "bvh") {
        std::cerr << "[warn] unknown accel \"" << name << "\", using bvh\n";
    }
    if (type == scene.accelType()) return;
    auto t0 = std::chrono::high_resolution_clock::now();
    scene.setAccel(type);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cerr << "[accel] " << name << " built in " << ms << " ms\n";
}

std::unique_ptr<Integrator> makeIntegrator(const std::string& name,
                                           int maxDepth,
                                           const Color& bg) {
    if (name == "direct") {
        return std::unique_ptr<Integrator>(
            new DirectLightingIntegrator(maxDepth, bg));
    }
    if (name == "whitted") {
        return std::unique_ptr<Integrator>(
            new WhittedIntegrator(maxDepth, bg));
    }
    return std::unique_ptr<Integrator>(
        new PathTracingIntegrator(maxDepth, 3, bg));
}

bool hasDisplay() {
    const char* d = std::getenv("DISPLAY");
    const char* w = std::getenv("WAYLAND_DISPLAY");
    return (d && *d) || (w && *w);
}

std::shared_ptr<Material> maybeLoadPbr(const std::string& dir) {
    if (dir.empty()) return nullptr;
    auto mat = PbrMaterial::loadFromDirectory(dir);
    return std::static_pointer_cast<Material>(mat);
}

int runHeadless(const Options& o) {
    SceneSetup setup = buildScene(o);
    applyAccel(setup.scene, o.accel);
    // --bg overrides the scene's environment, which overrides the black default.
    Color bg = o.hasBg ? o.bgColor : setup.background;

    auto integrator = makeIntegrator(o.integratorName, o.maxDepth, bg);
    integrator->setEnvironment(resolveEnv(o, setup));

    Image image(o.width, o.height);
    Renderer renderer(o.spp);
    renderer.setThreadCount(o.threads);
    renderer.setSampling(o.sampler == "random" ? Renderer::Sampling::Random
                                               : Renderer::Sampling::Stratified);
    renderer.setPassCallback([&](int passIdx, int totalSpp) {
        if ((passIdx + 1) % 16 == 0 || passIdx + 1 == o.spp) {
            image.writePNG(o.outPath);
            std::cerr << "[render] pass " << (passIdx + 1) << "/" << o.spp
                      << " written to " << o.outPath << "\n";
        }
    });
    auto t0 = std::chrono::high_resolution_clock::now();
    renderer.render(setup.scene, setup.camera, *integrator, image);
    auto t1 = std::chrono::high_resolution_clock::now();

    image.writePNG(o.outPath);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cerr << "[done] " << o.spp << " spp @ " << o.width << "x" << o.height
              << " in " << ms << " ms -> " << o.outPath << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options o = parseArgs(argc, argv);
    bool wantGui = o.gui && !o.headless;
    if (!wantGui && !o.headless) {
        // auto: gui if display is available, otherwise headless.
        wantGui = hasDisplay();
    }

    if (!wantGui) {
        return runHeadless(o);
    }

    QApplication app(argc, argv);
    SceneSetup setup = buildScene(o);
    applyAccel(setup.scene, o.accel);
    Color bg = o.hasBg ? o.bgColor : setup.background;
    auto integrator = makeIntegrator(o.integratorName, o.maxDepth, bg);
    integrator->setEnvironment(resolveEnv(o, setup));

    Gui window(o.width, o.height, setup.scene, setup.camera, *integrator, o.spp);
    window.show();
    return app.exec();
}
