// octree over primitive pointers.
//
// space is split at each node's center into 8 equal octants; a primitive is
// referenced by every octant its bbox overlaps, so leaves may share prims (the
// closest-hit bookkeeping in the traversal keeps that correct). complements the
// BVH: object partitioning (bvh) vs space partitioning (octree) - see the
// --accel flag for a side-by-side comparison.

#pragma once

#include <vector>

#include "accel/accel_structure.h"
#include "accel/bbox.h"

class Octree : public AccelStructure {
public:
    void build(const std::vector<Primitive*>& prims) override;

    bool intersect(const Ray& ray, HitRecord& rec) const override;
    bool occluded(const Ray& ray) const override;

    size_t nodeCount() const { return m_nodes.size(); }

private:
    struct Node {
        BBox bounds;
        int children[8];     // node indices; -1 = absent. leaf iff all -1.
        int firstPrim = 0;   // range into m_primIndices (leaves only)
        int primCount = 0;
    };

    int buildNode(const BBox& bounds, const std::vector<int>& prims,
                  const std::vector<BBox>& bboxes, int depth);

    std::vector<Node> m_nodes;
    std::vector<Primitive*> m_prims;
    std::vector<int> m_primIndices;
};
