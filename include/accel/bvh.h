// sah-binned bvh over primitive pointers. flat node array for cache locality.

#pragma once

#include <memory>
#include <vector>

#include "accel/accel_structure.h"
#include "accel/bbox.h"
#include "geometry/primitive.h"

class BVH : public AccelStructure {
public:
    BVH() = default;

    void build(const std::vector<Primitive*>& prims);

    bool intersect(const Ray& ray, HitRecord& rec) const override;
    bool occluded(const Ray& ray) const;

    BBox bounds() const {
        return m_nodes.empty() ? BBox::empty() : m_nodes[0].bounds;
    }
    size_t primitiveCount() const { return m_prims.size(); }

private:
    struct Node {
        BBox bounds;
        int left = -1;
        int right = -1;
        int firstPrim = -1;
        int primCount = 0;
    };

    int buildRecursive(std::vector<int>& indices,
                       const std::vector<BBox>& bboxes,
                       const std::vector<glm::dvec3>& centers,
                       int start, int end, int depth);

    std::vector<Node> m_nodes;
    std::vector<Primitive*> m_prims;
};
