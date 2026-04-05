#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string_view>
typedef unsigned int uint;

struct Vector2
{
	float u, v;
	Vector2() : u(0), v(0) {}
	Vector2(float u, float v) : u(u), v(v) {}
	Vector2 operator*(float f) const { return Vector2(u * f, v * f); }
	Vector2 operator+(const Vector2& vec) const { return Vector2(u + vec.u, v + vec.v); }
	Vector2 operator-(const Vector2& vec) const { return Vector2(u - vec.u, v - vec.v); }
};

struct Vector3
{
	float x, y, z;
	Vector3() : x(0), y(0), z(0) {}
	Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
	float operator[](int i) const { return (&x)[i]; }
	float operator*(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
	Vector3 operator/(float f) const { return Vector3(x / f, y / f, z / f); }
	Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
	Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
	Vector3 operator*(float f) const { return Vector3(x * f, y * f, z * f); }
	friend Vector3 operator*(float f, const Vector3& v) { return Vector3(v.x * f, v.y * f, v.z * f); }
	double dot(Vector3 v1, Vector3 v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }
};

struct Vector4
{
	float x, y, z, w;
	Vector4(float x = 0, float y = 0, float z = 0, float w = 1) : x(x), y(y), z(z), w(w) {}
	Vector4(Vector3 v3, float w = 1) : x(v3.x), y(v3.y), z(v3.z), w(w) {}
	Vector4 operator/(float f) const { return { x / f, y / f, z / f, w / f }; }
	Vector4 operator+(Vector4 v) const { return { x + v.x, y + v.y, z + v.z, w + v.w }; }
	Vector4 operator-(Vector4 v) const { return { x - v.x, y - v.y, z - v.z, w - v.w }; }
};

struct FinalVertex
{
	float x, y, z;       // Pos
	float u, v;          // TexCoord
	float nx, ny, nz;    // Normal
};

class OBJLoader
{
public : 
	bool LoadFile(const std::string& filepath)
	{
		std::ifstream file(filepath);
		if (!file.is_open())
		{
			std::cerr << "Error: Failed to open file" << std::endl;
			return false;
		}

		// 清空旧数据
		vertices_.clear();
		uv_.clear();
		render_buffer_.clear();

		vertices_.reserve(1000);
		render_buffer_.reserve(1000);

		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == '#') continue;//跳过空格和注释
			//逐行读取数据
			std::istringstream iss(line);
			std::string prefix;
			iss >> prefix;

			if (prefix == "v")
			{
				Vector3 v;
				iss >> v.x >> v.y >> v.z;
				vertices_.push_back(v);
			}
			else if (prefix == "vt")
			{
				Vector2 tex;
				iss >> tex.u >> tex.v;
				tex.v = 1.0f - tex.v;
				uv_.push_back(tex);
			}
			else if (prefix == "vn")
			{
				Vector3 n;
				iss >> n.x >> n.y >> n.z;
				normal_.push_back(n);
			}
			else if (prefix == "f")
			{
				ParseFace(iss);
			}
		}
		std::cout << "Model loaded. Vertices: " << vertices_.size()
			<< " Triangles: " << render_buffer_.size() / 3 << std::endl;

		return true;
		
	}

	void ParseFace(std::istringstream& iss)
	{
		std::string token;
		// 临时存储一个面的所有顶点，为了处理多边形（如四边形）
		std::vector<FinalVertex> faceVertices;

		while (iss >> token) {
			std::istringstream tokenStream(token);
			std::string indexStr;

			int vIdx = 0, vtIdx = 0, vnIdx = 0;

			//解析 v (位置)
			if (std::getline(tokenStream, indexStr, '/') && !indexStr.empty()) {
				vIdx = std::stoi(indexStr);
			}

			//解析 vt (纹理)
			if (std::getline(tokenStream, indexStr, '/') && !indexStr.empty()) {
				vtIdx = std::stoi(indexStr);
			}

			//解析 vn (法线)
			if (std::getline(tokenStream, indexStr, '/') && !indexStr.empty()) {
				vnIdx = std::stoi(indexStr);
			}

			FinalVertex vertex;
			//获取 Position
			if (vIdx > 0 && vIdx <= vertices_.size()) {
				vertex.x = vertices_[vIdx - 1].x;
				vertex.y = vertices_[vIdx - 1].y;
				vertex.z = vertices_[vIdx - 1].z;
			}

			//获取 UV
			if (vtIdx > 0 && vtIdx <= uv_.size()) {
				vertex.u = uv_[vtIdx - 1].u;
				vertex.v = uv_[vtIdx - 1].v;
			}
			else {
				vertex.u = 0.0f; vertex.v = 0.0f; // 默认值
			}

			//获取 Normal
			if (vnIdx > 0 && vnIdx <= normal_.size()) {
				vertex.nx = normal_[vnIdx - 1].x;
				vertex.ny = normal_[vnIdx - 1].y;
				vertex.nz = normal_[vnIdx - 1].z;
			}
			else {
				vertex.nx = 0.0f; vertex.ny = 0.0f; vertex.nz = 0.0f; // 默认值
			}

			faceVertices.push_back(vertex);
		}

		if (faceVertices.size() < 3) return;

		//三角剖分
		for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
			render_buffer_.push_back(faceVertices[0]);
			render_buffer_.push_back(faceVertices[i]);
			render_buffer_.push_back(faceVertices[i + 1]);
		}
	}
	const std::vector<Vector3>& GetVertices() const { return vertices_; }
	const std::vector<Vector2>& GetUV() const { return uv_; }
	const std::vector<Vector3>& GetNormal() const { return normal_; }
	const std::vector< FinalVertex>& GetRenderBuffer() const { return render_buffer_; }
private:
	std::vector<Vector3> vertices_ = {};
	std::vector<Vector2> uv_ = {};
	std::vector<Vector3> normal_ = {};
	// 最终组装好的数据
	std::vector<FinalVertex> render_buffer_ = {};
};

