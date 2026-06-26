//
// Created by LEI XU on 5/13/19.
//
#pragma once
#ifndef RAYTRACING_OBJECT_H
#define RAYTRACING_OBJECT_H

#include "Vector.hpp"
#include "global.hpp"
#include "Bounds3.hpp"
#include "Ray.hpp"
#include "Intersection.hpp"

class Object
{
public:
    Object() {}
    virtual ~Object() {}
    virtual bool intersect(const Ray& ray) = 0;
    virtual bool intersect(const Ray& ray, float &, uint32_t &) const = 0;
    virtual Intersection getIntersection(Ray _ray) = 0;
    virtual Intersection getIntersectionNoBVH(Ray _ray) { return getIntersection(_ray); }
    // any-hit 遮挡查询：在 (eps, tMax) 内存在任一交点即返回 true（用于阴影光线，可提前退出）。
    // 默认回退到最近交点；具体物体可重写为更快的任意命中遍历。
    virtual bool intersectP(const Ray& ray, double tMax)
    {
        Intersection it = getIntersection(ray);
        return it.happened && it.distance < tMax;
    }
    virtual void getSurfaceProperties(const Vector3f &, const Vector3f &, const uint32_t &, const Vector2f &, Vector3f &, Vector2f &) const = 0;
    virtual Vector3f evalDiffuseColor(const Vector2f &) const =0;
    virtual Bounds3 getBounds()=0;
};



#endif //RAYTRACING_OBJECT_H
