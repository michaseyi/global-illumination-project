#include "accel/linear_list.h"

#include "geometry/primitive.h"

bool LinearList::intersect(const Ray& ray, HitRecord& rec) const {
    double closest = ray.tMax;
    bool hit = false;
    for (const Primitive* p : m_prims) {
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

bool LinearList::occluded(const Ray& ray) const {
    for (const Primitive* p : m_prims) {
        HitRecord tmp;
        if (p->intersect(ray, tmp)) return true;
    }
    return false;
}
