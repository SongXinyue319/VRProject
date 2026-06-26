#pragma once

#include "BVH.hpp"
#include "Intersection.hpp"
#include "Material.hpp"
#include "OBJ_Loader.hpp"
#include "Object.hpp"
#include "Triangle.hpp"
#include <cassert>
#include <array>

inline bool rayTriangleIntersect(const Vector3f& v0, const Vector3f& v1,
                          const Vector3f& v2, const Vector3f& orig,
                          const Vector3f& dir, float& tnear, float& u, float& v)
{
    Vector3f edge1 = v1 - v0;
    Vector3f edge2 = v2 - v0;
    Vector3f pvec = crossProduct(dir, edge2);
    float det = dotProduct(edge1, pvec);
    if (det == 0)
        return false;

    Vector3f tvec = orig - v0;
    u = dotProduct(tvec, pvec);
    if (u < 0 || u > det)
        return false;

    Vector3f qvec = crossProduct(tvec, edge1);
    v = dotProduct(dir, qvec);
    if (v < 0 || u + v > det)
        return false;

    float invDet = 1 / det;

    tnear = dotProduct(edge2, qvec) * invDet;
    u *= invDet;
    v *= invDet;

    return true;
}

class Triangle : public Object
{
public:
    Vector3f v0, v1, v2; // vertices A, B ,C , counter-clockwise order
    Vector3f e1, e2;     // 2 edges v1-v0, v2-v0;
    Vector3f t0, t1, t2; // texture coords
    Vector3f normal;
    Material* m;

    Triangle(Vector3f _v0, Vector3f _v1, Vector3f _v2, Material* _m = nullptr)
        : v0(_v0), v1(_v1), v2(_v2), m(_m)
    {
        e1 = v1 - v0;
        e2 = v2 - v0;
        normal = normalize(crossProduct(e1, e2));
    }

    bool intersect(const Ray& ray) override;
    bool intersect(const Ray& ray, float& tnear,
                   uint32_t& index) const override;
    Intersection getIntersection(Ray ray) override;
    // 阴影/遮挡 any-hit 测试：不做背面剔除（单面遮挡物也能正确挡光），在 (eps,tMax) 内命中即真。
    bool intersectP(const Ray& ray, double tMax) override;
    void getSurfaceProperties(const Vector3f& P, const Vector3f& I,
                              const uint32_t& index, const Vector2f& uv,
                              Vector3f& N, Vector2f& st) const override
    {
        N = normal;
        //        throw std::runtime_error("triangle::getSurfaceProperties not
        //        implemented.");
    }
    Vector3f evalDiffuseColor(const Vector2f&) const override;
    Bounds3 getBounds() override;
};

class MeshTriangle : public Object
{
public:
    MeshTriangle(const std::string& filename)
    {
        objl::Loader loader;
        loader.LoadFile(filename);

        assert(loader.LoadedMeshes.size() == 1);
        auto mesh = loader.LoadedMeshes[0];

        Vector3f min_vert = Vector3f{std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity()};
        Vector3f max_vert = Vector3f{-std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity()};

        // 整个网格共用一个漫反射/光泽材质：开启高光（Ks>0、specularExponent>0），
        // 使 Phong 高光项真正生效，而非之前 Ks=0 的纯漫反射。
        Material* mesh_mat =
            new Material(MaterialType::DIFFUSE_AND_GLOSSY,
                         Vector3f(0.5, 0.5, 0.5), Vector3f(0, 0, 0));
        mesh_mat->Kd = 0.6;
        mesh_mat->Ks = 0.3;
        mesh_mat->specularExponent = 64;

        for (int i = 0; i < mesh.Vertices.size(); i += 3) {
            std::array<Vector3f, 3> face_vertices;
            for (int j = 0; j < 3; j++) {
                auto vert = Vector3f(mesh.Vertices[i + j].Position.X,
                                     mesh.Vertices[i + j].Position.Y,
                                     mesh.Vertices[i + j].Position.Z) *
                            60.f;
                face_vertices[j] = vert;

                min_vert = Vector3f(std::min(min_vert.x, vert.x),
                                    std::min(min_vert.y, vert.y),
                                    std::min(min_vert.z, vert.z));
                max_vert = Vector3f(std::max(max_vert.x, vert.x),
                                    std::max(max_vert.y, vert.y),
                                    std::max(max_vert.z, vert.z));
            }

            triangles.emplace_back(face_vertices[0], face_vertices[1],
                                   face_vertices[2], mesh_mat);
        }

        bounding_box = Bounds3(min_vert, max_vert);

        std::vector<Object*> ptrs;
        for (auto& tri : triangles)
            ptrs.push_back(&tri);

        bvh = new BVHAccel(ptrs, 1, BVHAccel::SplitMethod::SAH);
    }

    bool intersect(const Ray& ray) { return true; }

    bool intersect(const Ray& ray, float& tnear, uint32_t& index) const
    {
        bool intersect = false;
        for (uint32_t k = 0; k < triangles.size(); k++) {
            /*
            const Vector3f& v0 = vertices[vertexIndex[k * 3]];
            const Vector3f& v1 = vertices[vertexIndex[k * 3 + 1]];
            const Vector3f& v2 = vertices[vertexIndex[k * 3 + 2]];
            */
            const Vector3f v0 = triangles[k].v0;
            const Vector3f v1 = triangles[k].v1;
            const Vector3f v2 = triangles[k].v2;

            float t, u, v;
            if (rayTriangleIntersect(v0, v1, v2, ray.origin, ray.direction, t,
                                     u, v) &&t < tnear) {
                tnear = t;
                index = k;
                intersect = true;
            }
        }

        return intersect;
    }

    Bounds3 getBounds() { return bounding_box; }

    void getSurfaceProperties(const Vector3f& P, const Vector3f& I,
                              const uint32_t& index, const Vector2f& uv,
                              Vector3f& N, Vector2f& st) const
    {
        const Vector3f& v0 = vertices[vertexIndex[index * 3]];
        const Vector3f& v1 = vertices[vertexIndex[index * 3 + 1]];
        const Vector3f& v2 = vertices[vertexIndex[index * 3 + 2]];
        Vector3f e0 = normalize(v1 - v0);
        Vector3f e1 = normalize(v2 - v1);
        N = normalize(crossProduct(e0, e1));
        const Vector2f& st0 = stCoordinates[vertexIndex[index * 3]];
        const Vector2f& st1 = stCoordinates[vertexIndex[index * 3 + 1]];
        const Vector2f& st2 = stCoordinates[vertexIndex[index * 3 + 2]];
        st = st0 * (1 - uv.x - uv.y) + st1 * uv.x + st2 * uv.y;
    }

    Vector3f evalDiffuseColor(const Vector2f& st) const
    {
        float scale = 5;
        float pattern =
            (fmodf(st.x * scale, 1) > 0.5) ^ (fmodf(st.y * scale, 1) > 0.5);
        return lerp(Vector3f(0.815, 0.235, 0.031),
                    Vector3f(0.937, 0.937, 0.231), pattern);
    }

    Intersection getIntersection(Ray ray)
    {
        Intersection intersec;

        if (bvh) {
            intersec = bvh->Intersect(ray);
        }

        return intersec;
    }

    Intersection getIntersectionNoBVH(Ray ray) override
    {
        Intersection nearest;
        for (auto& tri : triangles) {
            Intersection hit = tri.getIntersection(ray);
            if (hit.happened && hit.distance < nearest.distance)
                nearest = hit;
        }
        return nearest;
    }

    // 遮挡查询委托给网格内层 BVH 的 any-hit 遍历（找到第一个 t<tMax 的遮挡即返回）。
    bool intersectP(const Ray& ray, double tMax) override
    {
        return bvh ? bvh->IntersectP(ray, tMax) : false;
    }

    Bounds3 bounding_box;
    std::unique_ptr<Vector3f[]> vertices;
    uint32_t numTriangles;
    std::unique_ptr<uint32_t[]> vertexIndex;
    std::unique_ptr<Vector2f[]> stCoordinates;

    std::vector<Triangle> triangles;

    BVHAccel* bvh;

    Material* m;
};

inline bool Triangle::intersect(const Ray& ray) { return true; }
inline bool Triangle::intersect(const Ray& ray, float& tnear,
                                uint32_t& index) const
{
    return false;
}

inline Bounds3 Triangle::getBounds() { return Union(Bounds3(v0, v1), v2); }

inline Intersection Triangle::getIntersection(Ray ray)
{
    Intersection inter;
    // Möller–Trumbore 光线-三角形求交：解 O + t*D = (1-u-v)V0 + u*V1 + v*V2
    // 注：不再背面剔除——对闭合网格的最近可见面无影响，但能让阴影光线正确命中朝向光源的面。
    Vector3f pvec = crossProduct(ray.direction, e2);
    float det = dotProduct(e1, pvec);
    if (fabs(det) < EPSILON)        // 行列式≈0：光线与三角形平行，无交点
        return inter;

    float invDet = 1.0f / det;
    Vector3f tvec = ray.origin - v0;
    float u = dotProduct(tvec, pvec) * invDet;
    if (u < 0 || u > 1)             // 重心坐标越界
        return inter;

    Vector3f qvec = crossProduct(tvec, e1);
    float v = dotProduct(ray.direction, qvec) * invDet;
    if (v < 0 || u + v > 1)
        return inter;

    float t = dotProduct(e2, qvec) * invDet;
    if (t < 0)                      // 交点在光线起点之后，丢弃
        return inter;

    inter.happened = true;
    inter.coords = ray(t);
    inter.normal = normal;
    inter.distance = t;
    inter.obj = this;
    inter.m = m;
    return inter;
}

inline Vector3f Triangle::evalDiffuseColor(const Vector2f&) const
{
    return Vector3f(0.5, 0.5, 0.5);
}

inline bool Triangle::intersectP(const Ray& ray, double tMax)
{
    // 与 getIntersection 一致的非背面剔除 Möller–Trumbore（两面都算），
    // 命中且 t∈(EPSILON,tMax) 即为遮挡。不能用 rayTriangleIntersect（其 u>det 判定隐含背面剔除）。
    Vector3f pvec = crossProduct(ray.direction, e2);
    float det = dotProduct(e1, pvec);
    if (fabs(det) < EPSILON) return false;
    float invDet = 1.0f / det;
    Vector3f tvec = ray.origin - v0;
    float u = dotProduct(tvec, pvec) * invDet;
    if (u < 0 || u > 1) return false;
    Vector3f qvec = crossProduct(tvec, e1);
    float v = dotProduct(ray.direction, qvec) * invDet;
    if (v < 0 || u + v > 1) return false;
    float t = dotProduct(e2, qvec) * invDet;
    return t > EPSILON && t < tMax;
}
