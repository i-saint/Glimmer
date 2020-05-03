#pragma once

static const float PI = 3.14159265f;

// some of these are based on OptiX SDK examples

// orthonormal basis
struct ONB
{
    void init(float3 n)
    {
        normal = n;
        binormal = normalize(abs(n.x) > abs(n.z) ? float3(-n.y, n.x, 0) : float3(0, -n.z, n.y));
        tangent = cross(binormal, normal);
    }

    float3 inverse_transform(float3 p)
    {
        return (tangent * p.x) + (binormal * p.y) + (normal * p.z);
    }

    float3 tangent;
    float3 binormal;
    float3 normal;
};

inline float3 cosine_sample_hemisphere(float u1, float u2)
{
    // Uniformly sample disk.
    float r = sqrt(u1);
    float phi = 2.0f * PI * u2;
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, sqrt(max(0.0f, 1.0f - (x * x) - (y * y))));
}

inline uint tea(uint val0, uint val1)
{
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for (uint n = 0; n < 4; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

// random float in [0.0f, 1.0f)
inline float rnd01(inout uint prev)
{
    const uint LCG_A = 1664525u;
    const uint LCG_C = 1013904223u;
    prev = (LCG_A * prev + LCG_C);
    return ((float)(prev & 0x00FFFFFF) / (float)0x01000000);
}
inline float rnd11(inout uint prev)
{
    return rnd01(prev) * 2.0f - 1.0f;
}
inline float rnd55(inout uint prev)
{
    return rnd01(prev) - 0.5f;
}


// "A Fast and Robust Method for Avoiding Self-Intersection":
// http://www.realtimerendering.com/raytracinggems/unofficial_RayTracingGems_v1.5.pdf
inline float3 offset_ray(const float3 p, const float3 n)
{
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float int_scale = 256.0f;

    int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);
    float3 p_i = float3(
        asfloat(asint(p.x) + (p.x < 0 ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + (p.y < 0 ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + (p.z < 0 ? -of_i.z : of_i.z)));
    return float3(
        abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}


inline float3 mul_p(float4x4 m, float3 v)
{
    return mul(m, float4(v, 1.0f)).xyz;
}
inline float3 mul_v(float4x4 m, float3 v)
{
    return mul(m, float4(v, 0.0f)).xyz;
}

// a & b must be normalized
inline float angle_between(float3 a, float3 b)
{
    return acos(clamp(dot(a, b), 0, 1));
}


inline float2 barycentric_interpolation(float2 barycentric, float2 p0, float2 p1, float2 p2)
{
    return p0 + ((p1 - p0) * barycentric.x) + ((p2 - p0) * barycentric.y);
}
inline float3 barycentric_interpolation(float2 barycentric, float3 p0, float3 p1, float3 p2)
{
    return p0 + ((p1 - p0) * barycentric.x) + ((p2 - p0) * barycentric.y);
}
inline float4 barycentric_interpolation(float2 barycentric, float4 p0, float4 p1, float4 p2)
{
    return p0 + ((p1 - p0) * barycentric.x) + ((p2 - p0) * barycentric.y);
}


