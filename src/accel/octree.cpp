// octree build + stack traversal.

#include "accel/octree.h"

#include <algorithm>
#include <cmath>

#include "geometry/primitive.h"

namespace {

// subdivision stops at a handful of prims per leaf or when the tree is deep
// enough that further splits mostly duplicate straddling primitives.
constexpr int kLeafSize = 8;
constexpr int kMaxDepth = 12;

bool overlaps(const BBox& a, const BBox& b) {
    return a.min().x <= b.max().x && b.min().x <= a.max().x &&
           a.min().y <= b.max().y && b.min().y <= a.max().y &&
           a.min().z <= b.max().z && b.min().z <= a.max().z;
}

BBox octant(const BBox& b, int i) {
    const glm::dvec3 lo = b.min(), c = b.center(), hi = b.max();
    glm::dvec3 mn((i & 1) ? c.x : lo.x,
                  (i & 2) ? c.y : lo.y,
                  (i & 4) ? c.z : lo.z);
    glm::dvec3 mx((i & 1) ? hi.x : c.x,
                  (i & 2) ? hi.y : c.y,
                  (i & 4) ? hi.z : c.z);
    return BBox(mn, mx);
}

}  // namespace

void Octree::build(const std::vector<Primitive*>& prims) {
    m_prims = prims;
    m_nodes.clear();
    m_primIndices.clear();
    if (m_prims.empty()) return;

    std::vector<BBox> bboxes(m_prims.size());
    BBox root = BBox::empty();
    std::vector<int> all(m_prims.size());
    for (size_t i = 0; i < m_prims.size(); ++i) {
        bboxes[i] = m_prims[i]->bbox();
        root = root.unionWith(bboxes[i]);
        all[i] = int(i);
    }
    // pad the root a touch so prims lying exactly on the boundary stay inside.
    glm::dvec3 pad = 1e-6 * (root.extent() + glm::dvec3(1.0));
    root = BBox(root.min() - pad, root.max() + pad);

    m_nodes.reserve(m_prims.size() / 2 + 16);
    buildNode(root, all, bboxes, 0);
}

int Octree::buildNode(const BBox& bounds, const std::vector<int>& prims,
                      const std::vector<BBox>& bboxes, int depth) {
    int index = int(m_nodes.size());
    m_nodes.push_back(Node{});
    m_nodes[index].bounds = bounds;
    std::fill(std::begin(m_nodes[index].children),
              std::end(m_nodes[index].children), -1);

    if (int(prims.size()) <= kLeafSize || depth >= kMaxDepth) {
        m_nodes[index].firstPrim = int(m_primIndices.size());
        m_nodes[index].primCount = int(prims.size());
        m_primIndices.insert(m_primIndices.end(), prims.begin(), prims.end());
        return index;
    }

    for (int i = 0; i < 8; ++i) {
        BBox childBounds = octant(bounds, i);
        std::vector<int> childPrims;
        for (int p : prims) {
            if (overlaps(childBounds, bboxes[p])) childPrims.push_back(p);
        }
        if (childPrims.empty()) continue;
        // when a child inherits everything the split separated nothing; force
        // that child to become a leaf by entering it at the depth cutoff.
        int child = buildNode(childBounds, childPrims, bboxes,
                              childPrims.size() == prims.size() ? kMaxDepth
                                                                : depth + 1);
        // buildNode may grow m_nodes, so write through the index, not a ref.
        m_nodes[index].children[i] = child;
    }
    return index;
}

bool Octree::intersect(const Ray& ray, HitRecord& rec) const {
    if (m_nodes.empty()) return false;
    const glm::dvec3 invDir(1.0 / ray.direction.x,
                            1.0 / ray.direction.y,
                            1.0 / ray.direction.z);
    double closest = ray.tMax;
    bool hit = false;

    // manual stack of (node, entry t); children are visited front-to-back so
    // the closest-hit prune kicks in early.
    struct Entry { int node; double t; };
    Entry stack[256];
    int sp = 0;
    double t0;
    if (!m_nodes[0].bounds.intersect(ray.origin, invDir, ray.tMin, closest, t0))
        return false;
    stack[sp++] = {0, t0};

    while (sp > 0) {
        Entry e = stack[--sp];
        if (e.t > closest) continue;
        const Node& node = m_nodes[e.node];

        if (node.primCount > 0) {
            for (int k = 0; k < node.primCount; ++k) {
                const Primitive* p = m_prims[m_primIndices[node.firstPrim + k]];
                Ray local = ray;
                local.tMax = closest;
                HitRecord tmp;
                if (p->intersect(local, tmp) && tmp.t < closest) {
                    closest = tmp.t;
                    rec = tmp;
                    hit = true;
                }
            }
            continue;
        }

        // gather children the ray touches, sort near-to-far, push far first.
        Entry order[8];
        int n = 0;
        for (int i = 0; i < 8; ++i) {
            int c = node.children[i];
            if (c < 0) continue;
            double t;
            if (m_nodes[c].bounds.intersect(ray.origin, invDir, ray.tMin,
                                            closest, t)) {
                order[n++] = {c, t};
            }
        }
        std::sort(order, order + n,
                  [](const Entry& a, const Entry& b) { return a.t < b.t; });
        for (int i = n - 1; i >= 0 && sp < 256; --i) stack[sp++] = order[i];
    }
    return hit;
}

bool Octree::occluded(const Ray& ray) const {
    if (m_nodes.empty()) return false;
    const glm::dvec3 invDir(1.0 / ray.direction.x,
                            1.0 / ray.direction.y,
                            1.0 / ray.direction.z);
    int stack[256];
    int sp = 0;
    double t;
    if (!m_nodes[0].bounds.intersect(ray.origin, invDir, ray.tMin, ray.tMax, t))
        return false;
    stack[sp++] = 0;

    while (sp > 0) {
        const Node& node = m_nodes[stack[--sp]];
        if (node.primCount > 0) {
            for (int k = 0; k < node.primCount; ++k) {
                const Primitive* p = m_prims[m_primIndices[node.firstPrim + k]];
                HitRecord tmp;
                if (p->intersect(ray, tmp)) return true;  // any hit ends it
            }
            continue;
        }
        for (int i = 0; i < 8; ++i) {
            int c = node.children[i];
            if (c < 0) continue;
            double tc;
            if (m_nodes[c].bounds.intersect(ray.origin, invDir, ray.tMin,
                                            ray.tMax, tc) && sp < 256) {
                stack[sp++] = c;
            }
        }
    }
    return false;
}
