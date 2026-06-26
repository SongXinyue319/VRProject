//
// Created by LEI XU on 5/16/19.
//

#ifndef RAYTRACING_BOUNDS3_H
#define RAYTRACING_BOUNDS3_H
#include "Ray.hpp"
#include "Vector.hpp"
#include <limits>
#include <array>

class Bounds3
{
  public:
    Vector3f pMin, pMax; // two points to specify the bounding box
    Bounds3()
    {
        double minNum = std::numeric_limits<double>::lowest();
        double maxNum = std::numeric_limits<double>::max();
        pMax = Vector3f(minNum, minNum, minNum);
        pMin = Vector3f(maxNum, maxNum, maxNum);
    }
    Bounds3(const Vector3f p) : pMin(p), pMax(p) {}
    Bounds3(const Vector3f p1, const Vector3f p2)
    {
        pMin = Vector3f(fmin(p1.x, p2.x), fmin(p1.y, p2.y), fmin(p1.z, p2.z));
        pMax = Vector3f(fmax(p1.x, p2.x), fmax(p1.y, p2.y), fmax(p1.z, p2.z));
    }

    Vector3f Diagonal() const { return pMax - pMin; }
    int maxExtent() const
    {
        Vector3f d = Diagonal();
        if (d.x > d.y && d.x > d.z)
            return 0;
        else if (d.y > d.z)
            return 1;
        else
            return 2;
    }

    double SurfaceArea() const
    {
        Vector3f d = Diagonal();
        return 2 * (d.x * d.y + d.x * d.z + d.y * d.z);
    }

    Vector3f Centroid() { return 0.5 * pMin + 0.5 * pMax; }
    Bounds3 Intersect(const Bounds3& b)
    {
        return Bounds3(Vector3f(fmax(pMin.x, b.pMin.x), fmax(pMin.y, b.pMin.y),
                                fmax(pMin.z, b.pMin.z)),
                       Vector3f(fmin(pMax.x, b.pMax.x), fmin(pMax.y, b.pMax.y),
                                fmin(pMax.z, b.pMax.z)));
    }

    Vector3f Offset(const Vector3f& p) const
    {
        Vector3f o = p - pMin;
        if (pMax.x > pMin.x)
            o.x /= pMax.x - pMin.x;
        if (pMax.y > pMin.y)
            o.y /= pMax.y - pMin.y;
        if (pMax.z > pMin.z)
            o.z /= pMax.z - pMin.z;
        return o;
    }

    bool Overlaps(const Bounds3& b1, const Bounds3& b2)
    {
        bool x = (b1.pMax.x >= b2.pMin.x) && (b1.pMin.x <= b2.pMax.x);
        bool y = (b1.pMax.y >= b2.pMin.y) && (b1.pMin.y <= b2.pMax.y);
        bool z = (b1.pMax.z >= b2.pMin.z) && (b1.pMin.z <= b2.pMax.z);
        return (x && y && z);
    }

    bool Inside(const Vector3f& p, const Bounds3& b)
    {
        return (p.x >= b.pMin.x && p.x <= b.pMax.x && p.y >= b.pMin.y &&
                p.y <= b.pMax.y && p.z >= b.pMin.z && p.z <= b.pMax.z);
    }
    inline const Vector3f& operator[](int i) const
    {
        return (i == 0) ? pMin : pMax;
    }

    inline bool IntersectP(const Ray& ray, const Vector3f& invDir,
                           const std::array<int, 3>& dirisNeg) const;
    // 返回光线进入该盒的参数 t_enter（命中且 t_exit>=0 时）；未命中返回 +inf。
    // 供 BVH 有序遍历与最近距离剪枝使用（t_enter 可为负，表示起点在盒内）。
    inline float IntersectT(const Ray& ray, const Vector3f& invDir,
                            const std::array<int, 3>& dirIsNeg) const;
};



inline float Bounds3::IntersectT(const Ray& ray, const Vector3f& invDir,
                                 const std::array<int, 3>& dirIsNeg) const
{
    // invDir=(1/dx,1/dy,1/dz)（乘法替代除法）。用与符号无关的逐分量 min/max，
    // 当某方向分量为 0（invDir=±inf）且起点落在该 slab 内时自然得到 (-inf,+inf) 无约束，
    // 避免 swap 写法在该退化情形下误判未命中（dirIsNeg 参数保留以兼容调用方，此实现已不需要）。
    (void)dirIsNeg;
    Vector3f t1 = (pMin - ray.origin) * invDir;
    Vector3f t2 = (pMax - ray.origin) * invDir;
    float t_enter = std::max({std::min(t1.x, t2.x), std::min(t1.y, t2.y), std::min(t1.z, t2.z)});
    float t_exit  = std::min({std::max(t1.x, t2.x), std::max(t1.y, t2.y), std::max(t1.z, t2.z)});

    // 用 <= / >= 放宽，兼容轴对齐零厚度退化盒与相切情形
    if (t_exit < 0 || t_enter > t_exit)
        return std::numeric_limits<float>::infinity();
    return t_enter;
}

inline bool Bounds3::IntersectP(const Ray& ray, const Vector3f& invDir,
                                const std::array<int, 3>& dirIsNeg) const
{
    return IntersectT(ray, invDir, dirIsNeg) < std::numeric_limits<float>::infinity();
}

inline Bounds3 Union(const Bounds3& b1, const Bounds3& b2)
{
    Bounds3 ret;
    ret.pMin = Vector3f::Min(b1.pMin, b2.pMin);
    ret.pMax = Vector3f::Max(b1.pMax, b2.pMax);
    return ret;
}

inline Bounds3 Union(const Bounds3& b, const Vector3f& p)
{
    Bounds3 ret;
    ret.pMin = Vector3f::Min(b.pMin, p);
    ret.pMax = Vector3f::Max(b.pMax, p);
    return ret;
}

#endif // RAYTRACING_BOUNDS3_H
