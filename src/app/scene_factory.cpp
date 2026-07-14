// reusable scene presets.

#include "app/scene_factory.h"

#include <cmath>
#include <iostream>
#include <memory>

#include "geometry/geometry.h"
#include "shading/shading.h"
#include "core/constants.h"
#include "io/obj_loader.h"

namespace {

// the five colored walls + ceiling area light shared by the cornell presets.
// box bounds: x in [-1,1], y in [0,2], z in [-1,1]. left wall red, right wall
// green, the rest white; a small emissive quad just below the ceiling.
void addCornellShell(Scene& scene) {
    auto white = std::make_shared<LambertMaterial>(Color(0.73, 0.73, 0.73));
    auto red   = std::make_shared<LambertMaterial>(Color(0.65, 0.05, 0.05));
    auto green = std::make_shared<LambertMaterial>(Color(0.12, 0.45, 0.15));

    // floor
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 0.0,  1.0),
        glm::dvec3( 2.0, 0.0,  0.0),
        glm::dvec3( 0.0, 0.0, -2.0), white));
    // ceiling
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 2.0, -1.0),
        glm::dvec3( 2.0, 0.0,  0.0),
        glm::dvec3( 0.0, 0.0,  2.0), white));
    // back wall
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 0.0, -1.0),
        glm::dvec3( 2.0, 0.0,  0.0),
        glm::dvec3( 0.0, 2.0,  0.0), white));
    // left wall (red)
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 0.0,  1.0),
        glm::dvec3( 0.0, 0.0, -2.0),
        glm::dvec3( 0.0, 2.0,  0.0), red));
    // right wall (green)
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3( 1.0, 0.0, -1.0),
        glm::dvec3( 0.0, 0.0,  2.0),
        glm::dvec3( 0.0, 2.0,  0.0), green));

    auto lightQuad = std::make_shared<Quad>(
        glm::dvec3(-0.30, 1.99, -0.30),
        glm::dvec3( 0.60, 0.00,  0.00),
        glm::dvec3( 0.00, 0.00,  0.60),
        std::make_shared<EmissiveMaterial>(Color(17.0, 12.0, 4.0)));
    auto light = std::make_shared<AreaLight>(lightQuad,
                                             Color(17.0, 12.0, 4.0), false);
    scene.addAreaLight(lightQuad, light);
}

// add a single axis-aligned box (centered at `center`, with `size` and
// `yawDeg` rotation around y) as 12 triangles.
void addBox(Scene& scene,
            const glm::dvec3& center,
            const glm::dvec3& size,
            double yawDeg,
            std::shared_ptr<Material> mat) {
    double yaw = yawDeg * constants::kPi / 180.0;
    double cs = std::cos(yaw), sn = std::sin(yaw);
    auto rot = [&](glm::dvec3 v) {
        return glm::dvec3(cs * v.x + sn * v.z, v.y, -sn * v.x + cs * v.z);
    };

    glm::dvec3 h = size * 0.5;
    glm::dvec3 c[8];
    for (int i = 0; i < 8; ++i) {
        glm::dvec3 lc((i & 1) ? h.x : -h.x,
                      (i & 2) ? h.y : -h.y,
                      (i & 4) ? h.z : -h.z);
        c[i] = center + rot(lc);
    }
    // faces with outward-ccw winding (verified by hand)
    int faces[6][4] = {
        {0,1,5,4}, // -y
        {2,6,7,3}, // +y
        {0,4,6,2}, // -x
        {1,3,7,5}, // +x
        {0,2,3,1}, // -z
        {4,5,7,6}  // +z
    };
    for (int f = 0; f < 6; ++f) {
        auto* q = faces[f];
        scene.addPrimitive(std::make_shared<Triangle>(c[q[0]], c[q[1]], c[q[2]], mat));
        scene.addPrimitive(std::make_shared<Triangle>(c[q[0]], c[q[2]], c[q[3]], mat));
    }
}

} // namespace

SceneSetup SceneFactory::createStarterScene(int width, int height) {
    Scene scene;

    auto red   = std::make_shared<LambertMaterial>(Color(0.8, 0.2, 0.2));
    auto green = std::make_shared<LambertMaterial>(Color(0.2, 0.8, 0.2));
    auto gray  = std::make_shared<LambertMaterial>(Color(0.7, 0.7, 0.7));

    scene.addPrimitive(std::make_shared<Sphere>(
        glm::dvec3(-0.35, -0.10, -4.4), 0.9, red));
    scene.addPrimitive(std::make_shared<Sphere>(
        glm::dvec3(0.95, -0.35, -5.9), 0.65, green));
    scene.addPrimitive(std::make_shared<Triangle>(
        glm::dvec3(-4.2, -1.0, -2.8),
        glm::dvec3( 4.2, -1.0, -2.8),
        glm::dvec3( 0.0, -1.0, -9.5),
        gray));

    scene.addLight(std::make_shared<PointLight>(
        glm::dvec3(3.0, 4.0, 0.0), Color(1.0, 1.0, 1.0), 25.0));

    scene.build();

    Camera camera(
        glm::dvec3(0.0, 0.5, 1.5),
        glm::dvec3(0.0, 0.0, -4.0),
        glm::dvec3(0.0, 1.0, 0.0),
        45.0, width, height
    );
    return {std::move(scene), camera};
}

SceneSetup SceneFactory::createCornellBoxScene(int width, int height,
                                               CornellSphere kind,
                                               std::shared_ptr<Material> sphereOverride) {
    Scene scene;

    auto white = std::make_shared<LambertMaterial>(Color(0.73, 0.73, 0.73));
    auto red   = std::make_shared<LambertMaterial>(Color(0.65, 0.05, 0.05));
    auto green = std::make_shared<LambertMaterial>(Color(0.12, 0.45, 0.15));

    // box bounds: x in [-1,1], y in [0,2], z in [-1,1]
    // floor
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 0.0,  1.0),
        glm::dvec3( 2.0, 0.0,  0.0),
        glm::dvec3( 0.0, 0.0, -2.0),
        white));
    // ceiling
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 2.0, -1.0),
        glm::dvec3( 2.0, 0.0,  0.0),
        glm::dvec3( 0.0, 0.0,  2.0),
        white));
    // back wall (z = -1, normal +z)
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 0.0, -1.0),
        glm::dvec3( 2.0, 0.0,  0.0),
        glm::dvec3( 0.0, 2.0,  0.0),
        white));
    // left wall (x = -1, normal +x): red
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3(-1.0, 0.0,  1.0),
        glm::dvec3( 0.0, 0.0, -2.0),
        glm::dvec3( 0.0, 2.0,  0.0),
        red));
    // right wall (x = +1, normal -x): green
    scene.addPrimitive(std::make_shared<Quad>(
        glm::dvec3( 1.0, 0.0, -1.0),
        glm::dvec3( 0.0, 0.0,  2.0),
        glm::dvec3( 0.0, 2.0,  0.0),
        green));

    // tall white box on the left, slightly rotated.
    addBox(scene,
           glm::dvec3(-0.40, 0.6, -0.30),
           glm::dvec3( 0.55, 1.2,  0.55),
           18.0,
           white);

    // sphere on the right side.
    std::shared_ptr<Material> sphereMat;
    if (sphereOverride) {
        sphereMat = sphereOverride;
    } else {
        switch (kind) {
            case CornellSphere::Glass:
                sphereMat = std::make_shared<DielectricMaterial>(1.0, 1.5);
                break;
            case CornellSphere::RoughMetal:
                sphereMat = std::make_shared<ConductorMaterial>(
                    Color(0.95, 0.78, 0.45), 0.18);
                break;
            case CornellSphere::Diffuse:
            default:
                sphereMat = std::make_shared<LambertMaterial>(
                    Color(0.75, 0.75, 0.75));
                break;
        }
    }
    scene.addPrimitive(std::make_shared<Sphere>(
        glm::dvec3(0.40, 0.42, 0.30), 0.42, sphereMat));

    // ceiling area light: 0.6 x 0.6 quad just below the ceiling.
    auto lightQuad = std::make_shared<Quad>(
        glm::dvec3(-0.30, 1.99, -0.30),
        glm::dvec3( 0.60, 0.00,  0.00),
        glm::dvec3( 0.00, 0.00,  0.60),
        std::make_shared<EmissiveMaterial>(Color(17.0, 12.0, 4.0)));
    auto light = std::make_shared<AreaLight>(lightQuad,
                                             Color(17.0, 12.0, 4.0), false);
    scene.addAreaLight(lightQuad, light);

    scene.build();

    Camera camera(
        glm::dvec3(0.0, 1.0,  3.0),   // eye
        glm::dvec3(0.0, 1.0,  0.0),   // target
        glm::dvec3(0.0, 1.0,  0.0),   // up
        40.0, width, height
    );
    return {std::move(scene), camera};
}

SceneSetup SceneFactory::createMeshScene(int width, int height,
                                         const std::string& objPath,
                                         MeshMaterial kind) {
    Scene scene;
    addCornellShell(scene);

    // FromFile shades each face with the .mtl the model ships with; the other
    // kinds override the whole mesh with one look. the material chosen here is
    // also the fallback for faces the file leaves unassigned.
    const bool useFileMaterials = (kind == MeshMaterial::FromFile);
    std::shared_ptr<Material> mat;
    switch (kind) {
        case MeshMaterial::Metal:
            // warm gold-ish rough conductor
            mat = std::make_shared<ConductorMaterial>(
                Color(1.0, 0.78, 0.34), 0.12);
            break;
        case MeshMaterial::Glass:
            mat = std::make_shared<DielectricMaterial>(1.0, 1.5);
            break;
        case MeshMaterial::Mirror:
            mat = std::make_shared<MirrorMaterial>(Color(0.95));
            break;
        case MeshMaterial::FromFile:
        case MeshMaterial::Diffuse:
        default:
            mat = std::make_shared<LambertMaterial>(Color(0.72, 0.72, 0.76));
            break;
    }

    // drop the model onto the floor (y = 0), centered, ~1.2 units across.
    ObjLoader loader;
    bool ok = loader.loadFitted(objPath, scene, mat,
                                glm::dvec3(0.0, 0.0, 0.0), 1.2, true,
                                useFileMaterials);
    if (!ok) {
        std::cerr << "[mesh] failed to load \"" << objPath
                  << "\" — rendering an empty box.\n";
    }

    scene.build();

    Camera camera(
        glm::dvec3(0.0, 1.0,  3.4),   // eye
        glm::dvec3(0.0, 0.5,  0.0),   // target
        glm::dvec3(0.0, 1.0,  0.0),   // up
        40.0, width, height
    );
    return {std::move(scene), camera};
}

SceneSetup SceneFactory::createGlassShowcaseScene(int width, int height) {
    Scene scene;
    addCornellShell(scene);

    auto glass       = std::make_shared<DielectricMaterial>(1.0, 1.5);
    auto glassTinted = std::make_shared<DielectricMaterial>(
        1.0, 1.5, Color(1.0), Color(0.80, 0.95, 1.0));
    auto mirror      = std::make_shared<MirrorMaterial>(Color(0.95, 0.95, 0.95));

    // back: large mirror sphere — picks up the whole box as a reflection.
    scene.addPrimitive(std::make_shared<Sphere>(
        glm::dvec3(-0.45, 0.55, -0.35), 0.55, mirror));
    // front-right: clear glass sphere.
    scene.addPrimitive(std::make_shared<Sphere>(
        glm::dvec3( 0.45, 0.36, 0.30), 0.36, glass));
    // front-left, smaller: faintly blue-tinted glass.
    scene.addPrimitive(std::make_shared<Sphere>(
        glm::dvec3(-0.10, 0.24, 0.55), 0.24, glassTinted));

    scene.build();

    Camera camera(
        glm::dvec3(0.0, 1.0,  3.0),
        glm::dvec3(0.0, 0.7,  0.0),
        glm::dvec3(0.0, 1.0,  0.0),
        40.0, width, height
    );
    return {std::move(scene), camera};
}
