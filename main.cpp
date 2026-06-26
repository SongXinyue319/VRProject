#include "Renderer.hpp"
#include "Scene.hpp"
#include "Triangle.hpp"
#include "Vector.hpp"
#include "global.hpp"
#include <chrono>
#include <string>

// In the main function of the program, we create the scene (create objects and
// lights) as well as set the options for the render (image width and height,
// maximum recursion depth, field-of-view, etc.). We then call the render
// function().
int main(int argc, char** argv)
{
    Scene scene(1280, 960);

    std::string model_path = "../models/bunny/bunny.obj";
    bool check_mode = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "check")
        {
            check_mode = true;
        }
        else if (arg.rfind("spp=", 0) == 0)
        {
            int s = std::atoi(arg.c_str() + 4);
            if (s >= 1) scene.spp = s;   // 每像素 NxN 超采样抗锯齿（默认 1）
        }
        else
        {
            model_path = arg;
        }
    }

    std::cout << "Model: " << model_path << "  (spp=" << scene.spp << ")\n";
    MeshTriangle bunny(model_path);

    scene.Add(&bunny);
    scene.Add(std::make_unique<Light>(Vector3f(-20, 70, 20), 1));
    scene.Add(std::make_unique<Light>(Vector3f(20, 70, 20), 1));

    if (check_mode)
    {
        std::cout << "Rendering using Check mode\n";
    }
    else
    {
        scene.buildBVH();
    }

    Renderer r;

    auto start = std::chrono::steady_clock::now();
    r.Render(scene, check_mode);
    auto stop = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = stop - start;

    std::cout << "Render complete: \n";
    std::cout << "Time taken: " << std::chrono::duration_cast<std::chrono::hours>(stop - start).count() << " hours\n";
    std::cout << "          : " << std::chrono::duration_cast<std::chrono::minutes>(stop - start).count() << " minutes\n";
    std::cout << "          : " << std::chrono::duration_cast<std::chrono::seconds>(stop - start).count() << " seconds\n";
    std::cout << "Render seconds: " << elapsed.count() << "\n";

    return 0;
}
