// Implementation of reusable scene presets for the starter project.

#include "app/scene_factory.h"

#include <memory>

#include "geometry/geometry.h"
#include "shading/shading.h"

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
        glm::dvec3(3.0, 4.0, 0.0),
        Color(1.0, 1.0, 1.0),
        25.0));

    Camera camera(
        glm::dvec3(0.0, 0.5, 1.5),   // eye
        glm::dvec3(0.0, 0.0, -4.0),  // target
        glm::dvec3(0.0, 1.0, 0.0),   // up
        45.0,                        // vertical FOV in degrees
        width,
        height
    );

    return {scene, camera};
}
