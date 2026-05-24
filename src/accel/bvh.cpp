// sah-binned bvh implementation. flat stack traversal.

#include "accel/bvh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr int kBins = 12;
constexpr int kLeafCutoff = 2;
constexpr int kMaxDepth = 64;

struct Bin {
    BBox bounds = BBox::empty();
    int count = 0;
};
}

void BVH::build(const std::vector<Primitive*>& prims) {
    m_prims = prims;
    m_nodes.clear();
    if (m_prims.empty()) return;

    std::vector<BBox> bboxes(m_prims.size());
    std::vector<glm::dvec3> centers(m_prims.size());
    std::vector<int> indices(m_prims.size());
    for (size_t i = 0; i < m_prims.size(); ++i) {
        bboxes[i] = m_prims[i]->bbox();
        centers[i] = bboxes[i].center();
        indices[i] = int(i);
    }

    m_nodes.reserve(m_prims.size() * 2);
    buildRecursive(indices, bboxes, centers, 0, int(m_prims.size()), 0);

    // re-pack primitives into the order chosen by the build so leaves point
    // at a contiguous range.
    std::vector<Primitive*> reordered(m_prims.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        reordered[i] = m_prims[indices[i]];
    }
    m_prims = std::move(reordered);
}

int BVH::buildRecursive(std::vector<int>& indices,
                        const std::vector<BBox>& bboxes,
                        const std::vector<glm::dvec3>& centers,
                        int start, int end, int depth) {
    int nodeIndex = int(m_nodes.size());
    m_nodes.emplace_back();

    BBox nodeBounds = BBox::empty();
    BBox centerBounds = BBox::empty();
    for (int i = start; i < end; ++i) {
        nodeBounds = nodeBounds.unionWith(bboxes[indices[i]]);
        centerBounds = centerBounds.unionWith(centers[indices[i]]);
    }
    m_nodes[nodeIndex].bounds = nodeBounds;

    int n = end - start;
    if (n <= kLeafCutoff || depth >= kMaxDepth) {
        m_nodes[nodeIndex].firstPrim = start;
        m_nodes[nodeIndex].primCount = n;
        return nodeIndex;
    }

    int axis = centerBounds.maxAxis();
    glm::dvec3 ext = centerBounds.extent();
    if (ext[axis] < 1e-12) {
        m_nodes[nodeIndex].firstPrim = start;
        m_nodes[nodeIndex].primCount = n;
        return nodeIndex;
    }

    std::array<Bin, kBins> bins;
    double invExt = double(kBins) / ext[axis];
    double low = centerBounds.min()[axis];
    for (int i = start; i < end; ++i) {
        int b = int((centers[indices[i]][axis] - low) * invExt);
        b = std::max(0, std::min(kBins - 1, b));
        bins[b].count += 1;
        bins[b].bounds = bins[b].bounds.unionWith(bboxes[indices[i]]);
    }

    std::array<double, kBins - 1> cost;
    std::array<BBox, kBins - 1> leftBounds;
    std::array<int,  kBins - 1> leftCount;
    std::array<BBox, kBins - 1> rightBounds;
    std::array<int,  kBins - 1> rightCount;

    {
        BBox acc = BBox::empty();
        int  cnt = 0;
        for (int i = 0; i < kBins - 1; ++i) {
            acc = acc.unionWith(bins[i].bounds);
            cnt += bins[i].count;
            leftBounds[i] = acc;
            leftCount[i] = cnt;
        }
    }
    {
        BBox acc = BBox::empty();
        int  cnt = 0;
        for (int i = kBins - 2; i >= 0; --i) {
            acc = acc.unionWith(bins[i + 1].bounds);
            cnt += bins[i + 1].count;
            rightBounds[i] = acc;
            rightCount[i] = cnt;
        }
    }

    double parentSA = nodeBounds.surfaceArea();
    double bestCost = std::numeric_limits<double>::infinity();
    int bestSplit = -1;
    for (int i = 0; i < kBins - 1; ++i) {
        if (leftCount[i] == 0 || rightCount[i] == 0) continue;
        double c = 0.125 +
                   (leftCount[i]  * leftBounds[i].surfaceArea()
                  + rightCount[i] * rightBounds[i].surfaceArea()) / parentSA;
        if (c < bestCost) {
            bestCost = c;
            bestSplit = i;
        }
    }

    double leafCost = double(n);
    if (bestSplit < 0 || bestCost >= leafCost) {
        m_nodes[nodeIndex].firstPrim = start;
        m_nodes[nodeIndex].primCount = n;
        return nodeIndex;
    }

    double splitPos = low + (bestSplit + 1) / double(kBins) * ext[axis];
    auto midIt = std::partition(indices.begin() + start, indices.begin() + end,
        [&](int idx) {
            return centers[idx][axis] < splitPos;
        });
    int mid = int(midIt - indices.begin());
    if (mid == start || mid == end) {
        mid = (start + end) / 2;
        std::nth_element(indices.begin() + start,
                         indices.begin() + mid,
                         indices.begin() + end,
                         [&](int a, int b) {
                             return centers[a][axis] < centers[b][axis];
                         });
    }

    int leftChild  = buildRecursive(indices, bboxes, centers, start, mid, depth + 1);
    int rightChild = buildRecursive(indices, bboxes, centers, mid, end, depth + 1);
    m_nodes[nodeIndex].left  = leftChild;
    m_nodes[nodeIndex].right = rightChild;
    return nodeIndex;
}

bool BVH::intersect(const Ray& ray, HitRecord& rec) const {
    if (m_nodes.empty()) return false;
    glm::dvec3 invDir(1.0 / ray.direction.x,
                      1.0 / ray.direction.y,
                      1.0 / ray.direction.z);

    int stack[64];
    int top = 0;
    stack[top++] = 0;

    bool hit = false;
    double closest = ray.tMax;

    while (top > 0) {
        int idx = stack[--top];
        const Node& node = m_nodes[idx];
        double tHit;
        if (!node.bounds.intersect(ray.origin, invDir, ray.tMin, closest, tHit))
            continue;

        if (node.primCount > 0) {
            for (int i = 0; i < node.primCount; ++i) {
                Ray local = ray;
                local.tMax = closest;
                HitRecord temp;
                if (m_prims[node.firstPrim + i]->intersect(local, temp)) {
                    if (temp.t < closest) {
                        closest = temp.t;
                        rec = temp;
                        hit = true;
                    }
                }
            }
        } else {
            // push the farther child first so the nearer is popped next
            int l = node.left;
            int r = node.right;
            double tl, tr;
            bool hl = m_nodes[l].bounds.intersect(ray.origin, invDir, ray.tMin, closest, tl);
            bool hr = m_nodes[r].bounds.intersect(ray.origin, invDir, ray.tMin, closest, tr);
            if (hl && hr) {
                if (tl < tr) { stack[top++] = r; stack[top++] = l; }
                else         { stack[top++] = l; stack[top++] = r; }
            } else if (hl) {
                stack[top++] = l;
            } else if (hr) {
                stack[top++] = r;
            }
        }
    }
    return hit;
}

bool BVH::occluded(const Ray& ray) const {
    if (m_nodes.empty()) return false;
    glm::dvec3 invDir(1.0 / ray.direction.x,
                      1.0 / ray.direction.y,
                      1.0 / ray.direction.z);
    int stack[64];
    int top = 0;
    stack[top++] = 0;
    while (top > 0) {
        int idx = stack[--top];
        const Node& node = m_nodes[idx];
        double tHit;
        if (!node.bounds.intersect(ray.origin, invDir, ray.tMin, ray.tMax, tHit))
            continue;
        if (node.primCount > 0) {
            for (int i = 0; i < node.primCount; ++i) {
                HitRecord temp;
                if (m_prims[node.firstPrim + i]->intersect(ray, temp)) {
                    return true;
                }
            }
        } else {
            stack[top++] = node.left;
            stack[top++] = node.right;
        }
    }
    return false;
}
