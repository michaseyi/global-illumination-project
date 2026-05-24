#include <QApplication>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "app/gui.h"
#include "app/scene_factory.h"
#include "io/image.h"
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
    int width = 600;
    int height = 600;
    int spp = 32;
    int maxDepth = 8;
    int threads = 0;
    bool gui = false;
    bool headless = false;
};

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
        if      (!std::strcmp(k, "--gui")) o.gui = true;
        else if (!std::strcmp(k, "--headless")) o.headless = true;
        else if (!std::strcmp(k, "--scene"))      { nextArg(argc, argv, i, v); o.scene = v; }
        else if (!std::strcmp(k, "--integrator")) { nextArg(argc, argv, i, v); o.integratorName = v; }
        else if (!std::strcmp(k, "--sphere"))     { nextArg(argc, argv, i, v); o.sphereKind = v; }
        else if (!std::strcmp(k, "--out"))        { nextArg(argc, argv, i, v); o.outPath = v; }
        else if (!std::strcmp(k, "--pbr-dir"))    { nextArg(argc, argv, i, v); o.pbrDir = v; }
        else if (!std::strcmp(k, "--w"))          { nextArg(argc, argv, i, v); o.width = std::atoi(v); }
        else if (!std::strcmp(k, "--h"))          { nextArg(argc, argv, i, v); o.height = std::atoi(v); }
        else if (!std::strcmp(k, "--spp"))        { nextArg(argc, argv, i, v); o.spp = std::atoi(v); }
        else if (!std::strcmp(k, "--max-depth"))  { nextArg(argc, argv, i, v); o.maxDepth = std::atoi(v); }
        else if (!std::strcmp(k, "--threads"))    { nextArg(argc, argv, i, v); o.threads = std::atoi(v); }
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
    Color bg(0.0);
    auto pbr = maybeLoadPbr(o.pbrDir);
    SceneSetup setup = (o.scene == "starter")
        ? SceneFactory::createStarterScene(o.width, o.height)
        : SceneFactory::createCornellBoxScene(o.width, o.height,
                                              parseSphere(o.sphereKind),
                                              pbr);

    auto integrator = makeIntegrator(o.integratorName, o.maxDepth, bg);

    Image image(o.width, o.height);
    Renderer renderer(o.spp);
    renderer.setThreadCount(o.threads);
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
    Color bg(0.0);
    auto pbr = maybeLoadPbr(o.pbrDir);
    SceneSetup setup = (o.scene == "starter")
        ? SceneFactory::createStarterScene(o.width, o.height)
        : SceneFactory::createCornellBoxScene(o.width, o.height,
                                              parseSphere(o.sphereKind),
                                              pbr);
    auto integrator = makeIntegrator(o.integratorName, o.maxDepth, bg);

    Gui window(o.width, o.height, setup.scene, setup.camera, *integrator, o.spp);
    window.show();
    return app.exec();
}
