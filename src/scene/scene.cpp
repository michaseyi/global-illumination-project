// scene wiring: a pluggable accel structure over bounded shapes (bvh by
// default, octree / brute-force via setAccel) + a linear scan for unbounded
// ones (planes).

#include "scene/scene.h"

#include "accel/bvh.h"
#include "accel/linear_list.h"
#include "accel/octree.h"
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
    switch (m_accelType) {
        case AccelType::Octree: m_accel.reset(new Octree()); break;
        case AccelType::Linear: m_accel.reset(new LinearList()); break;
        case AccelType::BVH:
        default:                m_accel.reset(new BVH()); break;
    }
    m_accel->build(bounded);
    m_built = true;
}

void Scene::setAccel(AccelType type) {
    if (type == m_accelType && m_built) return;
    m_accelType = type;
    if (m_built) build();
}

bool Scene::intersect(const Ray& ray, HitRecord& rec) const {
    bool hit = m_accel && m_accel->intersect(ray, rec);
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
    if (m_accel && m_accel->occluded(ray)) return true;
    for (Primitive* p : m_unbounded) {
        HitRecord tmp;
        if (p->intersect(ray, tmp)) return true;
    }
    return false;
}
