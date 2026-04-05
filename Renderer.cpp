#include <cmath>
#include <tuple>
#include <limits>
#include <cstdlib>
#include <random>
#include "RenderObjects.h"
#include "Shaders.h"

int main(int argc, char** argv)
{
    TGAColor bgColor = { 200, 200, 200, 255 };
    TGAImage framebuffer(width, height, TGAImage::RGB, bgColor);

    Viewport(width, height, 0, 0);
    zbuffer.resize(width * height, -std::numeric_limits<double>::max());
    init_zbuffer(width, height);
	const ConstantBuffer& cb{ Vector3(-1, 0, 2), Vector3(0, 0, 0), Vector3(0, 1, 0) };

    std::vector<double> shadowBuffer(width * height, -std::numeric_limits<double>::max());
    std::vector<double> cameraZBuffer(width * height, -std::numeric_limits<double>::max());
    // 获取光源空间矩阵
    auto lightSpaceMatrix = ShadowTransform::GetLightSpaceMatrix(cb.lightDir, cb.lightCenter, cb.lightUp);

    std::vector<RenderObjects> sceneObjects;

    //加载第一个模型
    RenderObjects diablo;
    if (diablo.Load("Models/diablo3_pose/", "diablo3_pose"))
        sceneObjects.push_back(std::move(diablo)); // 使用 std::move 避免深拷贝，提高性能

    //加载第二个模型
    RenderObjects floor;
    if (floor.Load("Models/floor/", "floor"))
        sceneObjects.push_back(std::move(floor));

    std::fill(zbuffer.begin(), zbuffer.end(), -1000.0);

    for (const auto& obj : sceneObjects)
    {
        const auto& render_buffer = obj.mesh.GetRenderBuffer();
        Shader_Depth depthShader(obj.mesh, lightSpaceMatrix);
        for (int i = 0; i < render_buffer.size(); i += 3)
        {
            std::array<Vector4, 3> clip = depthShader.vertex(i);
            rasterize(clip, depthShader, framebuffer);
        }
    }
    // 所有物体画完后，统一保存阴影图
    shadowBuffer = zbuffer;

    std::fill(zbuffer.begin(), zbuffer.end(), -1000.0);
    //遍历所有模型
    for (const auto& obj : sceneObjects)
    {
        const auto& render_buffer = obj.mesh.GetRenderBuffer();

        //清空全局 zbuffer，准备为玩家相机使用
        Shader_NormalMap shader(
            obj.mesh,
            render_buffer,
            obj.material.normalMap,
            obj.material.diffuseMap,
            obj.material.specMap,
            obj.material.glowMap,
            cb, lightSpaceMatrix,
            shadowBuffer);
        LookAt(shader.cb.eye, shader.cb.center, shader.cb.up);
        Perspective(length(shader.cb.eye - shader.cb.center));
        for (int i = 0; i < render_buffer.size(); i += 3)
        {
            std::array<Vector4, 3> clip = shader.vertex(i);
            //同时更新 framebuffer 和 zbuffer
            rasterize(clip, shader, framebuffer);
        }     
    }

	//随机值生成器，用于SSAO采样
    constexpr double ao_radius = .1;  // ssao ball radius in normalized device coordinates
    constexpr int nsamples = 128;     // number of samples in the ball
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-ao_radius, ao_radius);
    auto smoothstep = [](double edge0, double edge1, double x)
        {         // smoothstep returns 0 if the input is less than the left edge,
            double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);  // 1 if the input is greater than the right edge,
            return t * t * (3 - 2 * t);                                        // Hermite interpolation inbetween. The derivative of the smoothstep function is zero at both edges.
        };

#pragma omp parallel for
    //SSAO采样
	for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            Vector4 pos = viewportInverse * Vector4(x, y, z, 1.0f);
            double vote = 0;
            double voters = 0;
            for (int i = 0; i < nsamples; ++i)
            {
                Vector4 samplePos = viewport * (pos + Vector4(dist(gen), dist(gen), dist(gen), 0));
                if (samplePos.x < 0 || samplePos.x >= width || samplePos.y < 0 || samplePos.y >= height) continue;
                double d = zbuffer[int(samplePos.x) + int(samplePos.y) * width];
                if (z + 5 * ao_radius < d) continue;
                voters++;
                vote += d > samplePos.z;
            }
            double ssao = smoothstep(0, 1, 1 - vote / voters * .4);
            TGAColor c = framebuffer.get(x, y);
            //framebuffer.set(x, y, {static_cast<unsigned char>(ssao * 255), static_cast<unsigned char>(ssao * 255), static_cast<unsigned char>(ssao * 255), c[3]});
            framebuffer.set(x, y, { static_cast<unsigned char>(c[0] * ssao), static_cast<unsigned char>(c[1] * ssao), static_cast<unsigned char>(c[2] * ssao), c[3] });
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}