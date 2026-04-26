// Texture interface and a minimal constant texture.
// Extend this file when adding procedural or image textures.

#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "core/color.h"

class Texture {
public:
    virtual ~Texture() = default;

    virtual Color value(const glm::dvec2& uv,
                        const glm::dvec3& position) const = 0;
};

class ConstantTexture : public Texture {
public:
    explicit ConstantTexture(const Color& color) : m_color(color) {}

    Color value(const glm::dvec2&, const glm::dvec3&) const override {
        return m_color;
    }

private:
    Color m_color;
};