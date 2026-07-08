// multithreaded, progressive tile renderer.

#pragma once

#include <functional>

class Scene;
class Camera;
class Integrator;
class Image;

class Renderer {
public:
    // sampling strategy for pixel/lens and the integrator's path dimensions.
    enum class Sampling { Random, Stratified };

    explicit Renderer(int samplesPerPixel = 1);

    // optional callback invoked after each completed sample pass:
    //   passIndex (0-based), totalSpp.
    using PassCallback = std::function<void(int passIndex, int totalSpp)>;

    void setPassCallback(PassCallback cb) { m_passCallback = std::move(cb); }
    void setTileSize(int s) { m_tileSize = s > 0 ? s : 32; }
    void setThreadCount(int n) { m_threadCount = n; }
    void setSampling(Sampling s) { m_sampling = s; }

    void render(const Scene& scene,
                const Camera& camera,
                const Integrator& integrator,
                Image& image) const;

    int samplesPerPixel() const { return m_samplesPerPixel; }

private:
    int m_samplesPerPixel = 1;
    int m_tileSize = 32;
    int m_threadCount = 0; // 0 -> hardware concurrency
    Sampling m_sampling = Sampling::Stratified;
    PassCallback m_passCallback;
};
