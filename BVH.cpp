#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include "BVH.hpp"

// 取向量第 d 个分量。经 const 引用调用，确保走已定义的 const 版 operator[]
//（框架里非 const 的 Vector3f::operator[] 只声明未定义，直接对临时量取下标会链接失败）。
static inline float axisVal(const Vector3f& v, int d) { return (float)v[d]; }


BVHAccel::BVHAccel(std::vector<Object*> p, int maxPrimsInNode,
                   SplitMethod splitMethod)
    : maxPrimsInNode(std::min(255, maxPrimsInNode)), splitMethod(splitMethod),
      primitives(std::move(p))
{
    time_t start, stop;
    time(&start);
    if (primitives.empty())
        return;

    root = recursiveBuild(primitives);

    time(&stop);
    double diff = difftime(stop, start);
    int hrs = (int)diff / 3600;
    int mins = ((int)diff / 60) - (hrs * 60);
    int secs = (int)diff - (hrs * 3600) - (mins * 60);

    printf(
        "\rBVH Generation complete: \nTime Taken: %i hrs, %i mins, %i secs\n\n",
        hrs, mins, secs);
}

BVHBuildNode* BVHAccel::recursiveBuild(std::vector<Object*> objects)
{
    BVHBuildNode* node = new BVHBuildNode();

    if (objects.size() == 1) {
        // 叶节点
        node->bounds = objects[0]->getBounds();
        node->object = objects[0];
        node->left = nullptr;
        node->right = nullptr;
        return node;
    }
    else if (objects.size() == 2) {
        node->left = recursiveBuild(std::vector{objects[0]});
        node->right = recursiveBuild(std::vector{objects[1]});
        node->bounds = Union(node->left->bounds, node->right->bounds);
        return node;
    }
    else {
        // 沿质心包围盒最长轴划分
        Bounds3 centroidBounds;
        for (size_t i = 0; i < objects.size(); ++i)
            centroidBounds =
                Union(centroidBounds, objects[i]->getBounds().Centroid());
        int dim = centroidBounds.maxExtent();
        node->splitAxis = dim;

        // 先按质心在该轴上排序（中位数划分 / SAH 分桶划分都依赖位置有序）
        std::sort(objects.begin(), objects.end(), [dim](Object* a, Object* b) {
            return axisVal(a->getBounds().Centroid(), dim) <
                   axisVal(b->getBounds().Centroid(), dim);
        });

        size_t mid = objects.size() / 2; // 默认：中位数（NAIVE）

        // SAH（表面积启发式）分桶划分：枚举若干候选划分，取期望遍历代价最小者
        if (splitMethod == SplitMethod::SAH && objects.size() > 4) {
            constexpr int nBuckets = 12;
            int count[nBuckets] = {0};
            Bounds3 bbox[nBuckets];
            Bounds3 total;
            auto bucketOf = [&](Object* o) {
                float off =
                    axisVal(centroidBounds.Offset(o->getBounds().Centroid()), dim); // [0,1]
                int b = (int)(nBuckets * off);
                if (b >= nBuckets) b = nBuckets - 1;
                if (b < 0) b = 0;
                return b;
            };
            for (Object* o : objects) {
                int b = bucketOf(o);
                count[b]++;
                bbox[b] = Union(bbox[b], o->getBounds());
                total = Union(total, o->getBounds());
            }
            double bestCost = DBL_MAX;
            int bestSplit = -1;
            double invTotalSA = total.SurfaceArea() > 0 ? 1.0 / total.SurfaceArea() : 0;
            for (int i = 0; i < nBuckets - 1; ++i) {
                Bounds3 b0, b1;
                int c0 = 0, c1 = 0;
                for (int j = 0; j <= i; ++j) { c0 += count[j]; b0 = Union(b0, bbox[j]); }
                for (int j = i + 1; j < nBuckets; ++j) { c1 += count[j]; b1 = Union(b1, bbox[j]); }
                if (c0 == 0 || c1 == 0)
                    continue;
                double cost = 0.125 + (c0 * b0.SurfaceArea() + c1 * b1.SurfaceArea()) * invTotalSA;
                if (cost < bestCost) { bestCost = cost; bestSplit = i; }
            }
            if (bestSplit >= 0) {
                auto pmid = std::partition(
                    objects.begin(), objects.end(),
                    [&](Object* o) { return bucketOf(o) <= bestSplit; });
                size_t m = (size_t)(pmid - objects.begin());
                if (m != 0 && m != objects.size())
                    mid = m; // 否则退回中位数，避免空侧
            }
        }

        auto middling = objects.begin() + mid;
        auto leftshapes = std::vector<Object*>(objects.begin(), middling);
        auto rightshapes = std::vector<Object*>(middling, objects.end());
        assert(objects.size() == (leftshapes.size() + rightshapes.size()));

        node->left = recursiveBuild(leftshapes);
        node->right = recursiveBuild(rightshapes);
        node->bounds = Union(node->left->bounds, node->right->bounds);
    }

    return node;
}

Intersection BVHAccel::Intersect(const Ray& ray) const
{
    Intersection isect;
    if (!root)
        return isect;
    std::array<int, 3> dirIsNeg = {int(ray.direction.x > 0),
                                   int(ray.direction.y > 0),
                                   int(ray.direction.z > 0)};
    getIntersection(root, ray, ray.direction_inv, dirIsNeg, isect); // invDir 每条光线只算一次
    return isect;
}

// 有序遍历 + 最近距离剪枝：先下探近侧子树，再仅在远侧盒比当前最近交点更近时才下探。
void BVHAccel::getIntersection(BVHBuildNode* node, const Ray& ray,
                               const Vector3f& invDir,
                               const std::array<int, 3>& dirIsNeg,
                               Intersection& best) const
{
    float tBox = node->bounds.IntersectT(ray, invDir, dirIsNeg);
    if (!(tBox < best.distance)) // 未命中(+inf) 或盒子比已知最近交点更远 → 剪枝
        return;

    if (node->left == nullptr && node->right == nullptr) {
        Intersection it = node->object->getIntersection(ray);
        if (it.happened && it.distance < best.distance)
            best = it;
        return;
    }

    float tL = node->left->bounds.IntersectT(ray, invDir, dirIsNeg);
    float tR = node->right->bounds.IntersectT(ray, invDir, dirIsNeg);
    BVHBuildNode* first = (tL <= tR) ? node->left : node->right;
    BVHBuildNode* second = (tL <= tR) ? node->right : node->left;
    float tSecond = (tL <= tR) ? tR : tL;

    getIntersection(first, ray, invDir, dirIsNeg, best);
    if (tSecond < best.distance) // 远侧子树：仅当其盒入口比当前最近交点更近才下探
        getIntersection(second, ray, invDir, dirIsNeg, best);
}

bool BVHAccel::IntersectP(const Ray& ray, double tMax) const
{
    if (!root)
        return false;
    std::array<int, 3> dirIsNeg = {int(ray.direction.x > 0),
                                   int(ray.direction.y > 0),
                                   int(ray.direction.z > 0)};
    return intersectPNode(root, ray, ray.direction_inv, dirIsNeg, tMax);
}

// any-hit：找到第一个 t<tMax 的遮挡即返回 true（不追求最近，不做背面剔除）。
bool BVHAccel::intersectPNode(BVHBuildNode* node, const Ray& ray,
                              const Vector3f& invDir,
                              const std::array<int, 3>& dirIsNeg,
                              double tMax) const
{
    float tBox = node->bounds.IntersectT(ray, invDir, dirIsNeg);
    if (!(tBox < tMax)) // 未命中或盒子整体在 tMax 之后
        return false;

    if (node->left == nullptr && node->right == nullptr)
        return node->object->intersectP(ray, tMax);

    return intersectPNode(node->left, ray, invDir, dirIsNeg, tMax) ||
           intersectPNode(node->right, ray, invDir, dirIsNeg, tMax);
}
