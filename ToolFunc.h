#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <vector>
#include "ObjLoader.h" 
#include "tgaimage.h"
constexpr TGAColor  white = { 255, 255, 255, 255 };
constexpr TGAColor  green = { 0,   255, 0,   255 };
constexpr TGAColor    red = { 0,   0,   255, 255 };
constexpr TGAColor   blue = { 255, 0,   0,   255 };
constexpr TGAColor yellow = { 0,   200, 255, 255 };
constexpr float        PI = 3.1415926f;

inline std::vector<double> zbuffer;
inline Vector3 cross(Vector3 a, Vector3 b);
Vector3 normalized(Vector3 v);

template<int Rows, int Cols>
struct mat
{
    std::array<std::array<float, Cols>, Rows> data{};

    float& operator()(int r, int c) { return data[r][c]; }
    const float& operator()(int r, int c) const { return data[r][c]; }
    Vector3 operator*(const Vector3& v) const
    {
        // 如果是 4x4 矩阵，执行带齐次坐标 (w) 的投影变换
        if constexpr (Rows == 4 && Cols == 4)
        {
            float x = data[0][0] * v.x + data[0][1] * v.y + data[0][2] * v.z + data[0][3] * 1.0f;
            float y = data[1][0] * v.x + data[1][1] * v.y + data[1][2] * v.z + data[1][3] * 1.0f;
            float z = data[2][0] * v.x + data[2][1] * v.y + data[2][2] * v.z + data[2][3] * 1.0f;
            float w = data[3][0] * v.x + data[3][1] * v.y + data[3][2] * v.z + data[3][3] * 1.0f;

            if (std::abs(w) > 1e-5f)
                return Vector3(x / w, y / w, z / w);
            return Vector3(x, y, z);
        }
        // 如果是 3x3 矩阵，执行普通的线性变换（如法线乘以 TBN 矩阵）
        else if constexpr (Rows == 3 && Cols == 3)
        {
            float x = data[0][0] * v.x + data[0][1] * v.y + data[0][2] * v.z;
            float y = data[1][0] * v.x + data[1][1] * v.y + data[1][2] * v.z;
            float z = data[2][0] * v.x + data[2][1] * v.y + data[2][2] * v.z;
            return Vector3(x, y, z);
        }
        else
        {
            static_assert(Rows == 3 || Rows == 4, "Unsupported matrix dimensions for Vector3 multiplication");
            return Vector3();
        }
    }
   
    template<int NewCols>
    mat<Rows, NewCols> operator*(const mat<Cols, NewCols>& m) const
    {
        mat<Rows, NewCols> result;
        for (int i = 0; i < Rows; ++i)
        {
            for (int j = 0; j < NewCols; ++j)
            {
                result.data[i][j] = 0.0f;
                for (int k = 0; k < Cols; ++k)
                {
                    result.data[i][j] += data[i][k] * m.data[k][j];
                }
            }
        }
        return result;
    }
    Vector4 operator*(const Vector4& v) const
    {
        Vector4 res;
        res.x = data[0][0] * v.x + data[0][1] * v.y + data[0][2] * v.z + data[0][3] * v.w;
        res.y = data[1][0] * v.x + data[1][1] * v.y + data[1][2] * v.z + data[1][3] * v.w;
        res.z = data[2][0] * v.x + data[2][1] * v.y + data[2][2] * v.z + data[2][3] * v.w;
        res.w = data[3][0] * v.x + data[3][1] * v.y + data[3][2] * v.z + data[3][3] * v.w;
        return res;
    }
};

inline mat<4, 4> modelView, modelViewTransed, viewport, viewportInverse, perspective;

struct IShader
{
    virtual std::array<Vector4, 3> vertex(int idx) = 0;
    virtual bool fragment(const Vector3& bc, TGAColor& pixelColor) const = 0;
    virtual ~IShader() = default;
};

struct ConstantBuffer
{
     Vector3    eye{ -1.0f, 0.0f, 1.0f };  // camera position
     Vector3 center{ 0.0f, 0.0f, 0.0f }; // camera direction
     Vector3     up{ 0.0f, 1.0f, 0.0f };   // camera up vector
     
     Vector3 lightDir{ 0.5f, 2.5f, 2.0f };   // light direction
     Vector3  lightUp{ 0.0f, 1.0f, 0.0f };   // light up vector
     Vector3 lightCenter{ 0.0f, 0.0f, 0.0f };// light target point
};

class ShadowTransform
{
public :
    //构建光源视图矩阵
    [[nodiscard]] static mat<4, 4> CalculateLightView(const Vector3& lightDir,
        const Vector3& lightCenter,
        const Vector3& lightUp)
    {
        //计算虚拟光源位置：从中心点沿光线反方向后退一定距离 (比如后退 10 个单位)
        float distance = 30.0f;
        Vector3 lightEye = lightCenter + (normalized(lightDir) * distance);

        Vector3 n = normalized(lightEye - lightCenter); // Forward (Z)
        Vector3 l = normalized(cross(lightUp, n));      // Right (X)
        Vector3 m = normalized(cross(n, l));            // Up (Y)

        mat<4, 4> viewMat{};
        // 旋转部分 (正交基转置)
        viewMat(0, 0) = l.x; viewMat(0, 1) = l.y; viewMat(0, 2) = l.z;
        viewMat(1, 0) = m.x; viewMat(1, 1) = m.y; viewMat(1, 2) = m.z;
        viewMat(2, 0) = n.x; viewMat(2, 1) = n.y; viewMat(2, 2) = n.z;
        // 平移部分
        viewMat(0, 3) = -(l.x * lightEye.x + l.y * lightEye.y + l.z * lightEye.z);
        viewMat(1, 3) = -(m.x * lightEye.x + m.y * lightEye.y + m.z * lightEye.z);
        viewMat(2, 3) = -(n.x * lightEye.x + n.y * lightEye.y + n.z * lightEye.z);
        viewMat(3, 3) = 1.0f;

        return viewMat;
    }

    //构建光源正交投影矩阵
    [[nodiscard]] static mat<4, 4> CalculateOrtho(float left, float right,
        float bottom, float top,
        float zNear, float zFar)
    {
        mat<4, 4> ortho{};
        // 对角线：缩放分量
        ortho(0, 0) = 2.0f / (right - left);
        ortho(1, 1) = 2.0f / (top - bottom);
        ortho(2, 2) = 2.0f / (zFar - zNear); // 映射 Z 到 NDC (这里假设映射到 [-1, 1])
        ortho(3, 3) = 1.0f;

        // 第四列：平移分量
        ortho(0, 3) = -(right + left) / (right - left);
        ortho(1, 3) = -(top + bottom) / (top - bottom);
        ortho(2, 3) = (zFar + zNear) / (zFar - zNear);

        return ortho;
    }

    //获取 Light Space 矩阵
    [[nodiscard]] static mat<4, 4> GetLightSpaceMatrix(const Vector3& lightDir,
        const Vector3& lightCenter,
        const Vector3& lightUp)
    {
        mat<4, 4> lightView = CalculateLightView(lightDir, lightCenter, lightUp);

        // 正交投影的边界大小决定了阴影贴图能覆盖的物理范围。
        // 这里采用硬编码的范围，实际会根据相机的 Frustum 动态计算 (CSM技术)。
        float orthoSize = 15.0f;
        mat<4, 4> lightProj = CalculateOrtho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 100.0f);

        // 矩阵乘法：Projection * View
        return lightProj * lightView;
    }
};
           
//向量长度
inline float length(Vector3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
Vector3 normalized(Vector3 v);
//叉乘
inline Vector3 cross(Vector3 a, Vector3 b)
{
    return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
//有向面积
inline double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy)
{
    return .5 * ((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
}
Vector3 barycentric(const Vector2& a, const Vector2& b, const Vector2& c, const Vector2& p);
inline TGAColor vec3Totga(Vector3 v, unsigned char transparency)
{
    TGAColor pixelColor;
    pixelColor[2] = static_cast<unsigned char>(std::clamp(v.x * 255.0f, 0.0f, 255.0f)); // R
    pixelColor[1] = static_cast<unsigned char>(std::clamp(v.y * 255.0f, 0.0f, 255.0f)); // G
    pixelColor[0] = static_cast<unsigned char>(std::clamp(v.z * 255.0f, 0.0f, 255.0f)); // B
    pixelColor[3] = transparency;
    return pixelColor;
}
//提取法线信息
//注意1TGAColor的存储顺序是BGRA，而不是RGBA，所以n[0]是蓝色分量，n[1]是绿色分量，n[2]是红色分量
inline Vector3 UnpackNormal(const TGAColor& n)
{
    return Vector3(n[2] / 255.0f * 2.0f - 1.0f, n[1] / 255.0f * 2.0f - 1.0f, n[0] / 255.0f * 2.0f - 1.0f);
}
void Viewport(const int w, const int h, const int x, const int y);
void Perspective(const float f);
void LookAt(const Vector3 eye, const Vector3 center, const Vector3 up);
void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color);
void init_zbuffer(const int width, const int height);
void rasterize(std::array<Vector4, 3> clip, IShader& shader, TGAImage& framebuffer);

