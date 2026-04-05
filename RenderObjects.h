#pragma once
#include "tgaimage.h"
#include "ObjLoader.h"
//加载模型纹理资源
struct Materials
{
	TGAImage diffuseMap;
	TGAImage specMap;
	TGAImage normalMap;
	TGAImage glowMap;

	[[nodiscard]] bool Load(const std::string& basePath, const std::string& modelName)
	{
		bool allSuccess = true; // 记录是否所有贴图都加载成功

		// 拼接具体路径
		std::string diffusePath = basePath + modelName + "_diffuse.tga";
		std::string normalPath = basePath + modelName + "_nm_tangent.tga";
		std::string specPath = basePath + modelName + "_spec.tga";
		std::string glowPath = basePath + modelName + "_glow.tga";

		// 分别进行加载与错误检查
		if (!diffuseMap.read_tga_file(diffusePath))
		{
			std::cerr << "[Material Error] Failed to load Diffuse Map: " << diffusePath << std::endl;
			diffuseMap = TGAImage(2, 2, TGAImage::RGB, { 255, 0, 255 });
		}

		if (!normalMap.read_tga_file(normalPath))
		{
			std::cerr << "[Material Error] Failed to load Normal Map: " << normalPath << std::endl;
			normalMap = TGAImage(2, 2, TGAImage::RGB, { 0, 0, 0 });
		}

		if (!specMap.read_tga_file(specPath))
		{
			std::cerr << "[Material Error] Failed to load Specular Map: " << specPath << std::endl;
			specMap = TGAImage(2, 2, TGAImage::RGB, { 0, 0, 0 });
		}

		if (!glowMap.read_tga_file(glowPath)){
			std::cerr << "[Material Error] Failed to load Glow Map: " << glowPath << std::endl;
			glowMap = TGAImage(2, 2, TGAImage::RGB, { 0, 0, 0 });
		}
		return allSuccess;
	}
};

//加载一个模型实体
class RenderObjects
{
public :
	OBJLoader mesh;
	Materials material;

	[[nodiscard]] bool Load(const std::string& basePath, const std::string& modelName)
	{
		std::string objPath = basePath + modelName + ".obj";

		if (!mesh.LoadFile(objPath))
		{
			return false;
		}
		if (!material.Load(basePath, modelName))
		{
			return false;
		}
		return true;
	}
};

