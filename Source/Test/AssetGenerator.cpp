#include "pch.h"
#include "AssetGenerator.h"

template<class T>
void MakeCheckerImage(T* pixels, int width, int height, int block_size)
{
    T black = { 0.1f, 0.1f, 0.1f, 1.0f };
    T white = { 0.8f, 0.8f, 0.8f, 1.0f };
    for (int iy = 0; iy < height; iy++) {
        for (int ix = 0; ix < width; ix++) {
            int ip = iy * width + ix;
            int xb = ix / block_size;
            int yb = iy / block_size;

            if ((xb + yb) % 2 == 0)
                pixels[ip] = black;
            else
                pixels[ip] = white;
        }
    }
}
template void MakeCheckerImage<unorm8x4>(unorm8x4* pixels, int width, int height, int block_size);
template void MakeCheckerImage<half4>(half4* pixels, int width, int height, int block_size);
template void MakeCheckerImage<float4>(float4* pixels, int width, int height, int block_size);

template<class T>
void MakePolkaDotImage(T* pixels, int width, int height, int block_size)
{
    for (int iy = 0; iy < height; iy++) {
        for (int ix = 0; ix < width; ix++) {
            int ip = iy * width + ix;
            float2 pos = {
                (float(ix % block_size) / float(block_size)) * 2.0f - 1.0f,
                (float(iy % block_size) / float(block_size)) * 2.0f - 1.0f
            };
            float d = 1.0f - mu::length(pos);
            d = std::max(d - 0.3f, 0.0f) * (1.0f / 0.3f);
            float c = d;
            pixels[ip] = { c, c, c, 1.0f };
        }
    }
}
template void MakePolkaDotImage<unorm8x4>(unorm8x4* pixels, int width, int height, int block_size);
template void MakePolkaDotImage<half4>(half4* pixels, int width, int height, int block_size);
template void MakePolkaDotImage<float4>(float4* pixels, int width, int height, int block_size);


template<class T>
void MakeNormalMapFromHeightMap(T* dst, const T* src, int width, int height)
{
    for (int iy = 0; iy < height; iy++) {
        for (int ix = 0; ix < width; ix++) {
            int ip = iy * width + ix;
            auto sample = [&](int xo, int yo) -> float {
                int x = mu::clamp(ix + xo, 0, width - 1);
                int y = mu::clamp(iy + yo, 0, height - 1);
                return src[width * y + x].x;
            };

            float s01 = sample(-1, 0);
            float s21 = sample( 1, 0);
            float s10 = sample( 0,-1);
            float s12 = sample( 0, 1);
            auto va = mu::normalize(float3{ 2.0f, 0.0f, s21 - s01 });
            auto vb = mu::normalize(float3{ 0.0f, 2.0f, s10 - s12 });
            auto dir = mu::cross(va, vb);
            auto color = dir * 0.5f + 0.5f;
            dst[ip] = { color.x, color.y, color.z, 1.0f };
        }
    }
}
template void MakeNormalMapFromHeightMap<unorm8x4>(unorm8x4* dst, const unorm8x4* src, int width, int height);
template void MakeNormalMapFromHeightMap<half4>(half4* dst, const half4* src, int width, int height);
template void MakeNormalMapFromHeightMap<float4>(float4* dst, const float4* src, int width, int height);


void MakeCubeMesh(RawVector<int>& indices, RawVector<float3>& points, RawVector<float3>& normals, RawVector<float2>& uv, float size)
{
    const float s = size;
    points = {
        {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s},
        {-s, s,-s}, { s, s,-s}, { s, s, s}, {-s, s, s},
        {-s,-s,-s}, { s,-s,-s}, { s,-s, s}, {-s,-s, s},
        {-s,-s, s}, {-s,-s,-s}, {-s, s,-s}, {-s, s, s},
        { s,-s, s}, { s,-s,-s}, { s, s,-s}, { s, s, s},
        {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s},
    };
    normals = {
        { 0.0f, 0.0f, 1.0f}, { 0.0f, 0.0f, 1.0f}, { 0.0f, 0.0f, 1.0f}, { 0.0f, 0.0f, 1.0f},
        { 0.0f, 1.0f, 0.0f}, { 0.0f, 1.0f, 0.0f}, { 0.0f, 1.0f, 0.0f}, { 0.0f, 1.0f, 0.0f},
        { 0.0f,-1.0f, 0.0f}, { 0.0f,-1.0f, 0.0f}, { 0.0f,-1.0f, 0.0f}, { 0.0f,-1.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        { 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f},
        { 0.0f, 0.0f,-1.0f}, { 0.0f, 0.0f,-1.0f}, { 0.0f, 0.0f,-1.0f}, { 0.0f, 0.0f,-1.0f},
    };
    uv = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},
        {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f},
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
    };
    indices = {
        0,1,3, 3,1,2,
        5,4,6, 6,4,7,
        8,9,11, 11,9,10,
        13,12,14, 14,12,15,
        16,17,19, 19,17,18,
        21,20,22, 22,20,23,
    };
}

void MakeTorusMesh(RawVector<int>& indices, RawVector<float3>& points, RawVector<float3>& normals, RawVector<float2>& uv,
    float inner_radius, float outer_radius, int div1, int div2)
{
    float r = (outer_radius - inner_radius) / 2.0f;
    RawVector<float3> cpoints(div1);
    RawVector<float3> cnormals(div1);
    RawVector<float2> cuvs(div1);
    {
        float cx = (outer_radius + inner_radius) / 2.0f;
        float ca = (360.0f / div1) * mu::DegToRad;
        for (int pi = 0; pi < div1; ++pi) {
            float c = std::cos(ca * pi);
            float s = std::sin(ca * pi);
            cpoints[pi] = float3{ c * r + cx, s * r, 0.0f };
            cnormals[pi] = mu::normalize(float3{ c, s, 0.0f });
            cuvs[pi] = { 0.0f, (1.0f / (div1 - 1)) * pi };
        }
    }

    points.resize_discard(div1 * (div2 + 1));
    normals.resize_discard(div1 * (div2 + 1));
    uv.resize_discard(div1 * (div2 + 1));
    {
        float da = (360.0f / div2) * mu::DegToRad;
        for (int di = 0; di <= div2; ++di) {
            float3x3 rot = mu::to_mat3x3(mu::rotate_y(da * di));
            float2 uv_offset = { (1.0f / div2) * di, 0.0f };
            for (int pi = 0; pi < div1; ++pi) {
                int vi = div1 * di + pi;
                points[vi] = rot * cpoints[pi];
                normals[vi] = rot * cnormals[pi];
                uv[vi] = cuvs[pi] + uv_offset;
            }
        }
    }

    indices.resize_discard(div1 * div2 * 6);
    int* dsti = indices.data();
    for (int di = 0; di < div2; ++di) {
        for (int ci = 0; ci < div1; ++ci) {
            int i0 = ci + (div1 * di);
            int i1 = (ci + 1) % div1 + (div1 * di);
            int i2 = (ci + 1) % div1 + (div1 * (di + 1));
            int i3 = ci + (div1 * (di + 1));
            dsti[0] = i0;
            dsti[1] = i1;
            dsti[2] = i2;
            dsti[3] = i0;
            dsti[4] = i2;
            dsti[5] = i3;
            dsti += 6;
        }
    }
}

static inline int GetMiddlePoint(int p1, int p2, RawVector<float3>& points, std::map<int64_t, int>& cache, float radius)
{
    // first check if we have it already
    bool firstIsSmaller = p1 < p2;
    int64_t smallerIndex = firstIsSmaller ? p1 : p2;
    int64_t greaterIndex = firstIsSmaller ? p2 : p1;
    int64_t key = (smallerIndex << 32) + greaterIndex;

    {
        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }
    }

    // not in cache, calculate it
    const auto& point1 = points[p1];
    const auto& point2 = points[p2];
    float3 middle{
        (point1.x + point2.x) * 0.5f,
        (point1.y + point2.y) * 0.5f,
        (point1.z + point2.z) * 0.5f
    };

    // add vertex makes sure point is on unit sphere
    size_t i = points.size();
    points.push_back(normalize(middle) * radius);

    // store it, return index
    cache[key] = i;
    return i;
}

void MakeIcoSphereMesh(
    RawVector<int>& indices,
    RawVector<float3>& points,
    RawVector<float3>& normals,
    float radius,
    int iteration)
{
    float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    points = {
        normalize(float3{-1.0f,    t, 0.0f }) * radius,
        normalize(float3{ 1.0f,    t, 0.0f }) * radius,
        normalize(float3{-1.0f,   -t, 0.0f }) * radius,
        normalize(float3{ 1.0f,   -t, 0.0f }) * radius,
        normalize(float3{ 0.0f,-1.0f,    t }) * radius,
        normalize(float3{ 0.0f, 1.0f,    t }) * radius,
        normalize(float3{ 0.0f,-1.0f,   -t }) * radius,
        normalize(float3{ 0.0f, 1.0f,   -t }) * radius,
        normalize(float3{    t, 0.0f,-1.0f }) * radius,
        normalize(float3{    t, 0.0f, 1.0f }) * radius,
        normalize(float3{   -t, 0.0f,-1.0f }) * radius,
        normalize(float3{   -t, 0.0f, 1.0f }) * radius,
    };

    indices = {
        0, 11, 5,
        0, 5, 1,
        0, 1, 7,
        0, 7, 10,
        0, 10, 11,

        1, 5, 9,
        5, 11, 4,
        11, 10, 2,
        10, 7, 6,
        7, 1, 8,

        3, 9, 4,
        3, 4, 2,
        3, 2, 6,
        3, 6, 8,
        3, 8, 9,

        4, 9, 5,
        2, 4, 11,
        6, 2, 10,
        8, 6, 7,
        9, 8, 1,
    };

    std::map<int64_t, int> cache;
    for (int it = 0; it < iteration; it++) {
        RawVector<int> indices2;
        size_t n = indices.size();
        for (size_t fi = 0; fi < n; fi += 3)
        {
            int i1 = indices[fi + 0];
            int i2 = indices[fi + 1];
            int i3 = indices[fi + 2];
            int a = GetMiddlePoint(i1, i2, points, cache, radius);
            int b = GetMiddlePoint(i2, i3, points, cache, radius);
            int c = GetMiddlePoint(i3, i1, points, cache, radius);

            int addition[]{
                i1, a, c,
                i2, b, a,
                i3, c, b,
                a, b, c,
            };
            indices2.insert(indices2.end(), std::begin(addition), std::end(addition));
        }
        indices = indices2;
    }

    size_t n = points.size();
    normals.resize_discard(n);
    for (size_t i = 0; i < n; ++i) {
        normals[i] = mu::normalize(points[i]);
    }
}

void MakeWaveMesh(
    RawVector<int>& indices,
    RawVector<float3> &points,
    float size, float height,
    const int resolution,
    float angle)
{
    const int num_vertices = resolution * resolution;

    // vertices
    points.resize(num_vertices);
    for (int iy = 0; iy < resolution; ++iy) {
        for (int ix = 0; ix < resolution; ++ix) {
            int i = resolution*iy + ix;
            float2 pos = {
                float(ix) / float(resolution - 1) - 0.5f,
                float(iy) / float(resolution - 1) - 0.5f
            };
            float d = std::sqrt(pos.x*pos.x + pos.y*pos.y);

            float3& v = points[i];
            v.x = pos.x * size;
            v.y = std::sin(d * 10.0f + angle) * std::max<float>(1.0 - d, 0.0f) * height;
            v.z = pos.y * size;
        }
    }

    // topology
    {
        int num_faces = (resolution - 1) * (resolution - 1) * 2;
        int num_indices = num_faces * 3;

        indices.resize(num_indices);
        for (int iy = 0; iy < resolution - 1; ++iy) {
            for (int ix = 0; ix < resolution - 1; ++ix) {
                int i = (resolution - 1)*iy + ix;
                indices[i * 6 + 0] = resolution*iy + ix;
                indices[i * 6 + 1] = resolution*(iy + 1) + ix;
                indices[i * 6 + 2] = resolution*(iy + 1) + (ix + 1);
                indices[i * 6 + 3] = resolution*iy + ix;
                indices[i * 6 + 4] = resolution*(iy + 1) + (ix + 1);
                indices[i * 6 + 5] = resolution*iy + (ix + 1);
            }
        }
    }
}
