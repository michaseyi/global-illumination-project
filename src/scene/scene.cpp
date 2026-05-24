// scene wiring: bvh for bounded shapes + brute-force for unbounded ones.

#include "scene/scene.h"
#include "shading/light.h"

void Scene::addPrimitive(const std::shared_ptr<Primitive>& primitive) {
    m_primitives.push_back(primitive);
    m_built = false;
}

void Scene::addLight(const std::shared_ptr<Light>& light) {
    m_lights.push_back(light);
}

void Scene::addAreaLight(const std::shared_ptr<Primitive>& shape,
                         const std::shared_ptr<AreaLight>& light) {
    shape->setAreaLight(light.get());
    m_primitives.push_back(shape);
    m_emissive.push_back(shape);
    m_lights.push_back(std::static_pointer_cast<Light>(light));
    m_built = false;
}

void Scene::build() {
    std::vector<Primitive*> bounded;
    m_unbounded.clear();
    bounded.reserve(m_primitives.size());
    for (auto& p : m_primitives) {
        BBox b = p->bbox();
        if (b.isEmpty()) {
            m_unbounded.push_back(p.get());
        } else {
            bounded.push_back(p.get());
        }
    }
    m_bvh.build(bounded);
    m_built = true;
}

bool Scene::intersect(const Ray& ray, HitRecord& rec) const {
    bool hit = m_bvh.intersect(ray, rec);
    double closest = hit ? rec.t : ray.tMax;
    for (Primitive* p : m_unbounded) {
        Ray local = ray;
        local.tMax = closest;
        HitRecord tmp;
        if (p->intersect(local, tmp) && tmp.t < closest) {
            closest = tmp.t;
            rec = tmp;
            hit = true;
        }
    }
    return hit;
}

bool Scene::occluded(const Ray& ray) const {
    if (m_bvh.occluded(ray)) return true;
    for (Primitive* p : m_unbounded) {
        HitRecord tmp;
        if (p->intersect(ray, tmp)) return true;
    }
    return false;
}
