#pragma once
#include "tgaimage.h"
#include "ObjLoader.h"
#include "ToolFunc.h"

constexpr int width = 1024;
constexpr int height = 1024;

struct Shader : IShader
{
    OBJLoader& loader;
    const std::vector< FinalVertex>& render_buffer;
    std::array<Vector3, 3> varying_vertices;
    std::array<Vector3, 3> varying_normals;
    const ConstantBuffer& cb;

    Shader(OBJLoader& loader, const std::vector<FinalVertex>& rb) :
        loader(loader), render_buffer(rb), cb(cb) {}

    std::array<Vector4, 3> vertex(int idx) override
    {
        if (idx + 2 >= render_buffer.size()) return {};

        Vector3 v0(render_buffer[idx].x, render_buffer[idx].y, render_buffer[idx].z);
        Vector3 v1(render_buffer[idx + 1].x, render_buffer[idx + 1].y, render_buffer[idx + 1].z);
        Vector3 v2(render_buffer[idx + 2].x, render_buffer[idx + 2].y, render_buffer[idx + 2].z);
        varying_vertices = { v0, v1, v2 };

        Vector3 n0(render_buffer[idx].nx, render_buffer[idx].ny, render_buffer[idx].nz);
        Vector3 n1(render_buffer[idx + 1].nx, render_buffer[idx + 1].ny, render_buffer[idx + 1].nz);
        Vector3 n2(render_buffer[idx + 2].nx, render_buffer[idx + 2].ny, render_buffer[idx + 2].nz);
        varying_normals = { n0, n1, n2 };

        Vector4 a = perspective * modelView * Vector4(v0, 1);
        Vector4 b = perspective * modelView * Vector4(v1, 1);
        Vector4 c = perspective * modelView * Vector4(v2, 1);

        return std::array<Vector4, 3>{ a, b, c };
    }

    bool fragment(const Vector3& bc, TGAColor& pixelColor) const override
    {
        float ambient = 0.1f;

        Vector3    lightDir = normalized(cb.lightDir);
        TGAColor lighrColor = { 255, 255, 255, 255 };

        Vector3  center = (varying_vertices[0] + varying_vertices[1] + varying_vertices[2]) / 3.0f;
        Vector3 viewDir = normalized(cb.eye - center);


        Vector3  normal = varying_normals[0] * bc.x +
            varying_normals[1] * bc.y +
            varying_normals[2] * bc.z;
        normal = normalized(normal);
        float     NdotL = std::max(0.0f, static_cast<float>(normal * lightDir));
        float      diff = NdotL;

        Vector3 halfDir = normalized(viewDir + lightDir);
        float     NdotH = std::max(0.0f, static_cast<float>(normal * halfDir));
        float      spec = std::pow(NdotH, 32.0f);

        float     light = ambient + diff + (spec * 0.8f);
        // 防止过曝 (简单的截断)
        if (light > 1.0f) light = 1.0f;

        unsigned char c = static_cast<unsigned char>(255 * light);
        pixelColor = { c, c, c, 255 };
        return false;        //fasle: 不丢弃此像素
    }
};

struct Shader_NormalMap : IShader
{
    const OBJLoader& loader;
    const std::vector< FinalVertex>& render_buffer;
    const TGAImage& normalMap;
    const TGAImage& diffuseMap;
    const TGAImage& specMap;
    const TGAImage& glowMap;
    const ConstantBuffer& cb;
	mat<4, 4> lightSpaceMatrix;
    std::vector<double> shadowBuffer;

    std::array<Vector3, 3> varying_vertices;
    std::array<Vector3, 3> varying_normals;
    std::array<Vector2, 3> varying_uvs;

    Shader_NormalMap(const OBJLoader& loader, const std::vector<FinalVertex>& rb, const TGAImage& nm, const TGAImage& dm, 
                     const TGAImage& sm, const TGAImage& gm, const ConstantBuffer& cb, const mat<4, 4>& lightSpaceMatrix, std::vector<double>& shadowBuffer) :
        loader(loader), render_buffer(rb), normalMap(nm), diffuseMap(dm), specMap(sm), glowMap(gm), cb(cb), lightSpaceMatrix(lightSpaceMatrix),  shadowBuffer(shadowBuffer) {}

    std::array<Vector4, 3> vertex(int idx) override
    {
        if (idx + 2 >= render_buffer.size()) return {};

        for (int i = 0; i < 3; ++i)
        {
            varying_vertices[i] = { render_buffer[idx + i].x,  render_buffer[idx + i].y,  render_buffer[idx + i].z };
            varying_normals[i] = { render_buffer[idx + i].nx, render_buffer[idx + i].ny, render_buffer[idx + i].nz };
            varying_uvs[i] = { render_buffer[idx + i].u,  render_buffer[idx + i].v };
        }

        Vector4 a = perspective * modelView * Vector4(varying_vertices[0], 1.0f);
        Vector4 b = perspective * modelView * Vector4(varying_vertices[1], 1.0f);
        Vector4 c = perspective * modelView * Vector4(varying_vertices[2], 1.0f);

        return std::array<Vector4, 3>{ a, b, c };
    }

    bool fragment(const Vector3& bc, TGAColor& pixelColor) const override
    {
        /* ----------------解包贴图---------------- */
        //插值计算当前像素的顶点位置、法线和UV坐标
        Vector3  vertex = varying_vertices[0] * bc.x + varying_vertices[1] * bc.y + varying_vertices[2] * bc.z;
        Vector3  normal = varying_normals[0] * bc.x + varying_normals[1] * bc.y + varying_normals[2] * bc.z;
        Vector2      uv = varying_uvs[0] * bc.x + varying_uvs[1] * bc.y + varying_uvs[2] * bc.z;

        //采样漫反射贴图
        int u_diff = std::clamp(static_cast<int>(uv.u * diffuseMap.width()), 0, diffuseMap.width() - 1);
        int v_diff = std::clamp(static_cast<int>(uv.v * diffuseMap.height()), 0, diffuseMap.height() - 1);
        TGAColor diffuseColor = diffuseMap.get(u_diff, v_diff);

        //采样高光贴图
        int u_spec = std::clamp(static_cast<int>(uv.u * specMap.width()), 0, specMap.width() - 1);
        int v_spec = std::clamp(static_cast<int>(uv.v * specMap.height()), 0, specMap.height() - 1);
        TGAColor specularColor = specMap.get(u_spec, v_spec);

        //采样自发光贴图
        int u_glow = std::clamp(static_cast<int>(uv.u * glowMap.width()), 0, glowMap.width() - 1);
        int v_glow = std::clamp(static_cast<int>(uv.v * glowMap.height()), 0, glowMap.height() - 1);
        TGAColor glowColor = glowMap.get(u_glow, v_glow);

        //采样法线贴图
        int u_norm = std::clamp(static_cast<int>(uv.u * normalMap.width()), 0, normalMap.width() - 1);
        int v_norm = std::clamp(static_cast<int>(uv.v * normalMap.height()), 0, normalMap.height() - 1);
        TGAColor normalColor = normalMap.get(u_norm, v_norm);

        //将法线贴图的颜色值转换为[-1, 1]范围的法线向量
        Vector3 mappedNormal = UnpackNormal(normalColor);

        //构建TBN矩阵，将切线空间的法线转换到世界空间
        Vector3    edge1 = varying_vertices[1] - varying_vertices[0];
        Vector3    edge2 = varying_vertices[2] - varying_vertices[0];
        Vector2 deltaUV1 = varying_uvs[1] - varying_uvs[0];
        Vector2 deltaUV2 = varying_uvs[2] - varying_uvs[0];

        float f = 1.0f / (deltaUV1.u * deltaUV2.v - deltaUV2.u * deltaUV1.v);
        Vector3 tangent;
        tangent.x = f * (deltaUV2.v * edge1.x - deltaUV1.v * edge2.x);
        tangent.y = f * (deltaUV2.v * edge1.y - deltaUV1.v * edge2.y);
        tangent.z = f * (deltaUV2.v * edge1.z - deltaUV1.v * edge2.z);

        tangent = normalized(tangent - normal * (tangent * normal));
        Vector3 bitangent = normalized(cross(normal, tangent));

        mat<3, 3> TBN = { {{ {tangent.x, bitangent.x, normal.x},
                             {tangent.y, bitangent.y, normal.y},
                             {tangent.z, bitangent.z, normal.z} }} };

        mappedNormal = normalized(TBN * mappedNormal); // 将切线空间的法线转换到世界空间

        /* ----------------光照计算---------------- */
        //基础环境光
        Vector3 ambient(0.1f, 0.1f, 0.1f);
        // 初始化自发光颜色为黑色
        Vector3 glow(.0f, .0f, .0f);

        //将 TGA 的 0-255 映射到 0.0-1.0 空间 (注意 TGA 是 B G R 顺序)
        Vector3 albedo(diffuseColor[2] / 255.0f, diffuseColor[1] / 255.0f, diffuseColor[0] / 255.0f);

        Vector3    lightDir = normalized(cb.lightDir);
        TGAColor lighrColor = { 255, 128, 255, 255 };

        Vector3      center = (varying_vertices[0] + varying_vertices[1] + varying_vertices[2]) / 3.0f;
        Vector3     viewDir = normalized(cb.eye - center);

        float         NdotL = std::max(0.0f, static_cast<float>(mappedNormal * lightDir));
        Vector3     diffuse = albedo * NdotL;

        Vector3     halfDir = normalized(viewDir + lightDir);
        float         NdotH = std::max(0.0f, static_cast<float>(mappedNormal * halfDir));
        float      specMath = std::pow(NdotH, 8.0f);
        //采样 Spec 贴图作为强度
        float specIntensity = specularColor[0] /256.0f;
        Vector3 specular(specMath * specIntensity, specMath * specIntensity, specMath * specIntensity);

        //自发光
        if (!(glowColor[0] == 0 && glowColor[1] == 0 && glowColor[2] == 0))
        {
            float glowR = glowColor[glowColor.bytespp == 1 ? 0 : 2] / 255.0f;
            float glowG = glowColor[glowColor.bytespp == 1 ? 0 : 1] / 255.0f;
            float glowB = glowColor[0] / 255.0f;
            glow = Vector3(glowR, glowG, glowB) * 5.0f;
        }

        /* ----------------阴影计算---------------- */
        Vector3 worldPos = varying_vertices[0] * bc.x + varying_vertices[1] * bc.y + varying_vertices[2] * bc.z;
		float shadow = 1.0f; //亮度从0开始累加
        //将当前像素转换到光源 NDC 空间
        Vector4 fragPosLightSpace = lightSpaceMatrix * Vector4(worldPos, 1.0f);
        Vector3 projCoords(fragPosLightSpace.x / fragPosLightSpace.w,
                           fragPosLightSpace.y / fragPosLightSpace.w,
                           fragPosLightSpace.z / fragPosLightSpace.w);
        //将 NDC 坐标[-1, 1] 映射到屏幕坐标[0, width]
        int shadowX = static_cast<int>((projCoords.x + 1.0f) * 0.5f * width);
        int shadowY = static_cast<int>((projCoords.y + 1.0f) * 0.5f * height);

        float bias = 0.01f; // 阴影偏移，防止shadow acne

  //      //3x3邻域采样
		//float currDepth = projCoords.z;
		////遍历周围3x3像素
		//for (int y = -1; y <= 1; ++y)
  //      {
  //          for(int x = -1; x <= 1; ++x)
  //          {
		//		int sampleX = std::clamp(shadowX + x, 0, width - 1);
		//		int sampleY = std::clamp(shadowY + y, 0, height - 1);

  //              float closestDepth = shadowBuffer[sampleY * width + sampleX];

  //              // 如果光线没被挡住 (当前深度 >= 记录的最浅深度)，则贡献 1.0 的亮度
  //              if (currDepth + bias >= closestDepth)
  //                  shadow += 1.0f;
  //          }   
  //      }
  //      // 算出平均比例：9 次采样
  //      shadow /= 9.0f;

        //ao采样

        //深度对比
        if (shadowX >= 0 && shadowX < width && shadowY >= 0 && shadowY < height)
        {
            float closestDepth = shadowBuffer[shadowY * width + shadowX];
            float currentDepth = projCoords.z;
            //如果当前片元在光源视角下的深度大于阴影贴图中记录的深度，说明它被遮挡了
            if ((currentDepth + bias) < closestDepth) // 加入一个小偏移来减少阴影 acne
                shadow = 0.3f;
		}

        Vector3 finalColor = ambient + (diffuse + specular) * shadow + glow;

        pixelColor = vec3Totga(finalColor, 255);

        return false;        //fasle: 不丢弃此像素
    }
};

struct Shader_Depth : IShader {
    const OBJLoader& loader;
    const mat<4, 4>& lightSpaceMatrix;

    Shader_Depth(const OBJLoader& loader, const mat<4, 4>& lightSpaceMatrix)
        : loader(loader), lightSpaceMatrix(lightSpaceMatrix) {
    }

    std::array<Vector4, 3> vertex(int idx) override
    {
        if (idx + 2 >= loader.GetRenderBuffer().size()) return {};

        std::array<Vector4, 3> Position;
        for (int i = 0; i < 3; ++i)
        {
            Vector3 pos = { loader.GetRenderBuffer()[idx + i].x,
                            loader.GetRenderBuffer()[idx + i].y,
                            loader.GetRenderBuffer()[idx + i].z };

            //顶点直接乘以光源空间矩阵，完成从世界坐标到光源 NDC 坐标的转换
            Position[i] = lightSpaceMatrix * Vector4(pos, 1.0f);
        }
        return Position;
    }

    bool fragment(const Vector3& bc, TGAColor& pixelColor) const override
    {
        //rasterize() 函数会自动把 Z 值写进全局 zbuffer。
        return false;
    }
};