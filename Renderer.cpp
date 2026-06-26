//
// Created by goksu on 2/25/20.
//

#include <fstream>
#include <atomic>
#include "Scene.hpp"
#include "Renderer.hpp"
#ifdef _OPENMP
#include <omp.h>
#endif


inline float deg2rad(const float& deg) { return deg * M_PI / 180.0; }

const float EPSILON = 0.00001;

// The main render function. This where we iterate over all pixels in the image,
// generate primary rays and cast these rays into the scene. The content of the
// framebuffer is saved to a file.
void Renderer::Render(const Scene& scene,bool check_mode)
{
    std::vector<Vector3f> framebuffer(scene.width * scene.height);

    float scale = tan(deg2rad(scene.fov * 0.5));
    float imageAspectRatio = scene.width / (float)scene.height;
    Vector3f eye_pos(-1, 5, 10);

    const int spp = std::max(1, scene.spp);      // 每像素 spp×spp 网格超采样（spp=1 即像素中心单采样）
    const float inv_samples = 1.0f / (spp * spp);
    std::atomic<int> rows_done{0};

    // 每个像素相互独立，按行并行（castRay 为 const、无共享可变状态，线程安全）。
    // 写入用 framebuffer[j*width+i]（与迭代无关的索引），消除原 m++ 的跨迭代依赖。
#pragma omp parallel for schedule(dynamic, 4)
    for (int j = 0; j < scene.height; ++j) {
        for (int i = 0; i < scene.width; ++i) {
            Vector3f color(0, 0, 0);
            for (int sy = 0; sy < spp; ++sy) {
                for (int sx = 0; sx < spp; ++sx) {
                    // 子像素位置（spp=1 时退化为 i+0.5, j+0.5，即原始像素中心）
                    float px = i + (sx + 0.5f) / spp;
                    float py = j + (sy + 0.5f) / spp;
                    float x = (2.0f * px / scene.width - 1.0f) * imageAspectRatio * scale;
                    float y = (1.0f - 2.0f * py / scene.height) * scale;
                    Vector3f dir = normalize(Vector3f(x, y, -1));
                    Ray ray(eye_pos, dir);
                    color += check_mode ? scene.castRay_noBVH(ray, 0)
                                        : scene.castRay(ray, 0);
                }
            }
            framebuffer[j * scene.width + i] = color * inv_samples;
        }
        int d = ++rows_done;
#pragma omp critical
        UpdateProgress(d / (float)scene.height);
    }
    UpdateProgress(1.f);

    // save framebuffer to file
    FILE* fp = fopen("binary.ppm", "wb");
    (void)fprintf(fp, "P6\n%d %d\n255\n", scene.width, scene.height);
    for (auto i = 0; i < scene.height * scene.width; ++i) {
        unsigned char color[3];
        color[0] = (unsigned char)(255 * clamp(0, 1, framebuffer[i].x));
        color[1] = (unsigned char)(255 * clamp(0, 1, framebuffer[i].y));
        color[2] = (unsigned char)(255 * clamp(0, 1, framebuffer[i].z));
        fwrite(color, 1, 3, fp);
    }
    fclose(fp);
}
