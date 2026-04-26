// Renderer owns the outer image loop and delegates radiance work to an integrator.

#pragma once

class Scene;
class Camera;
class Integrator;
class Image;

class Renderer {
public:
    // samplesPerPixel is kept here so later assignments can extend the renderer
    // without changing the GUI/application path again.
    explicit Renderer(int samplesPerPixel = 1);

    void render(const Scene& scene,
                const Camera& camera,
                const Integrator& integrator,
                Image& image) const;

    int samplesPerPixel() const { return m_samplesPerPixel; }

private:
    int m_samplesPerPixel = 1;
};
