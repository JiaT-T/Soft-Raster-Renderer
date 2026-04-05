#include"ToolFunc.h"	
//归一化
Vector3 normalized(Vector3 v)
{
    double len = length(v);
    if (len == 0) return Vector3(0, 0, 0);
    return v / len;
}

//重心坐标
Vector3 barycentric(const Vector2& a, const Vector2& b, const Vector2& c, const Vector2& p)
{
    float areaABC = (b.u - a.u) * (c.v - a.v) - (c.u - a.u) * (b.v - a.v);

    // 如果面积非常接近 0，说明三角形退化（三点共线或面积为 0），直接拒绝
    if (std::abs(areaABC) < 1e-5f)
    {
        return { -1.0f, 1.0f, 1.0f }; // 返回负值表示在三角形外/无效
    }

    float areaPBC = (b.u - p.u) * (c.v - p.v) - (c.u - p.u) * (b.v - p.v);
    float areaPCA = (c.u - p.u) * (a.v - p.v) - (a.u - p.u) * (c.v - p.v);

    // 计算重心坐标
    float alpha = areaPBC / areaABC;
    float beta = areaPCA / areaABC;
    float gamma = 1.0f - alpha - beta;

    return { alpha, beta, gamma };
}

//视口变换
void Viewport(const int w, const int h, const int x, const int y)
{
    viewport = mat<4, 4>
    { {{
        {static_cast<float>(w) / 2.f, 0, 0, static_cast<float>(x + w / 2.f)},
        {0, static_cast<float>(h) / 2.f, 0, static_cast<float>(y + h / 2.f)},
        {0, 0, 1, 0},
        {0, 0, 0, 1}
    }} };

    const float inv_w = 2.0f / static_cast<float>(w);
    const float inv_h = 2.0f / static_cast<float>(h);

    viewportInverse = mat<4, 4>
    { {{
        { inv_w, 0.0f, 0.0f, -static_cast<float>(2 * x) / static_cast<float>(w) - 1.0f },
        { 0.0f,  inv_h, 0.0f, -static_cast<float>(2 * y) / static_cast<float>(h) - 1.0f },
        { 0.0f,  0.0f,  1.0f, 0.0f },
        { 0.0f,  0.0f,  0.0f, 1.0f }
    }} };
}

//透视投影
void Perspective(const float f)
{
    perspective = mat<4, 4>
    { {{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, -1.f / f, 1}
    }} };
}

//摄影机变换
void LookAt(const Vector3 eye, const Vector3 center, const Vector3 up)
{
    Vector3 n = normalized(eye - center);
    Vector3 l = normalized(cross(up, n));
    Vector3 m = normalized(cross(n, l));
    auto m1 = mat<4, 4>
    { {{
        {l.x, l.y, l.z, 0},
        {m.x, m.y, m.z, 0},
        {n.x, n.y, n.z, 0},
        {  0,   0,   0, 1}
    }} };

    auto m2 = mat<4, 4>
    { {{
        { 1, 0, 0, -center.x },
        { 0, 1, 0, -center.y },
        { 0, 0, 1, -center.z },
        { 0, 0, 0,     1     }
    }} };
    modelView = m1 * m2;
    modelViewTransed = mat<4, 4>
    { {{
        {l.x, m.x, n.x, center.x},
        {l.y, m.y, n.y, center.y},
        {l.z, m.z, n.z, center.z},
        {  0,   0,   0,    1    }
    }} };
}

//两点连线
void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color)
{
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    {
        std::swap(ax, ay);
        std::swap(bx, by);
    }

    if (ax > bx)
    {
        std::swap(ax, bx);
        std::swap(ay, by);
    }

    for (int x = ax; x <= bx; x++)
    {
        float t = (x - ax) / static_cast<float>(bx - ax);
        int y = std::round(ay + (by - ay) * t);
        if (steep)
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
    }
}

//初始化深度缓冲
void init_zbuffer(const int width, const int height)
{
    zbuffer = std::vector(width * height, -1000.);
}

void rasterize(std::array<Vector4, 3> clip, IShader& shader, TGAImage& framebuffer)
{
    //近平面剔除
    //注意：真正的工业级做法是进行裁剪 (Clipping) 生成新三角形，这里为了避免画面崩坏先做剔除
    if (clip[0].w <= 0.0f || clip[1].w <= 0.0f || clip[2].w <= 0.0f)
    {
        return;
    }

    std::array<Vector3, 3> ndc;
    std::array<Vector2, 3> screen;

    // 透视除法与视口变换
    for (int i = 0; i < 3; i++)
    {
        float inv_w = 1.0f / clip[i].w; // 乘法比除法快，提前算出倒数
        ndc[i].x = clip[i].x * inv_w;
        ndc[i].y = clip[i].y * inv_w;
        ndc[i].z = clip[i].z * inv_w;

        Vector4 v4(ndc[i], 1.0f);
        Vector4 res = viewport * v4;
        screen[i] = Vector2(res.x, res.y);
    }

    int width_m1 = framebuffer.width() - 1;
    int height_m1 = framebuffer.height() - 1;

    int aabbminx = std::clamp(static_cast<int>(std::floor(std::min({ screen[0].u, screen[1].u, screen[2].u }))), 0, width_m1);
    int aabbmaxx = std::clamp(static_cast<int>(std::ceil(std::max({ screen[0].u, screen[1].u, screen[2].u }))), 0, width_m1);
    int aabbminy = std::clamp(static_cast<int>(std::floor(std::min({ screen[0].v, screen[1].v, screen[2].v }))), 0, height_m1);
    int aabbmaxy = std::clamp(static_cast<int>(std::ceil(std::max({ screen[0].v, screen[1].v, screen[2].v }))), 0, height_m1);

    for (int y = aabbminy; y <= aabbmaxy; ++y)
    {
        for (int x = aabbminx; x <= aabbmaxx; ++x)
        {
            Vector2 P(static_cast<float>(x), static_cast<float>(y));
            Vector3 bc = barycentric(screen[0], screen[1], screen[2], P);

            // 剔除三角形外的像素
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            // 插值深度 Z
            double z = ndc[0].z * bc.x + ndc[1].z * bc.y + ndc[2].z * bc.z;

            int idx = x + y * framebuffer.width();
            if (z > zbuffer[idx])
            {
                TGAColor pixel_color;
                bool discard = shader.fragment(bc, pixel_color);

                // 如果Shader没有选择丢弃该像素
                if (!discard)
                {
                    zbuffer[idx] = z;
                    framebuffer.set(x, y, pixel_color);
                }
            }
        }
    }
}