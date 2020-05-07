#pragma once

static const float PI = 3.14159265f;
static const float FLT_EPSILON = 1.192092896e-07;
static const float FLT_MIN = 1.175494351e-38;
static const float FLT_MAX = 3.402823466e+38;

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

inline float3 onb_inverse_transform(float3 v, float3 n)
{
    ONB onb;
    onb.init(n);
    return onb.inverse_transform(v);
}


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
inline float3 rnd_dir(inout uint prev)
{
    return normalize(float3(rnd55(prev), rnd55(prev), rnd55(prev)));
}
inline float2 rnd_bc(inout uint prev)
{
    float2 r = float2(rnd55(prev), rnd55(prev));
    if (length(r > 1.0f))
        r = 1.0f - r;
    return r;
}


// "A Fast and Robust Method for Avoiding Self-Intersection":
// http://www.realtimerendering.com/raytracinggems/unofficial_RayTracingGems_v1.7.pdf
inline float3 offset_ray(float3 ray_pos, float3 face_normal)
{
    float origin = 1.0f / 32.0f;
    float float_scale = 1.0f / 65536.0f;
    float int_scale = 256.0f;

    int3 of_i = int3(
        int_scale * face_normal.x,
        int_scale * face_normal.y,
        int_scale * face_normal.z);
    float3 p_i = float3(
        asfloat(asint(ray_pos.x) + (ray_pos.x < 0 ? -of_i.x : of_i.x)),
        asfloat(asint(ray_pos.y) + (ray_pos.y < 0 ? -of_i.y : of_i.y)),
        asfloat(asint(ray_pos.z) + (ray_pos.z < 0 ? -of_i.z : of_i.z)));
    return float3(
        abs(ray_pos.x) < origin ? ray_pos.x + float_scale * face_normal.x : p_i.x,
        abs(ray_pos.y) < origin ? ray_pos.y + float_scale * face_normal.y : p_i.y,
        abs(ray_pos.z) < origin ? ray_pos.z + float_scale * face_normal.z : p_i.z);
}


inline bool near_equal(float a, float b, float t)
{
    return abs(a - b) <= t;
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

void swap(inout float a, inout float b) { float t = a; a = b; b = t; }
void swap(inout float2 a, inout float2 b) { float2 t = a; a = b; b = t; }
void swap(inout float3 a, inout float3 b) { float3 t = a; a = b; b = t; }
void swap(inout float4 a, inout float4 b) { float4 t = a; a = b; b = t; }
float length_sq(float2 a) { return dot(a, a); }
float length_sq(float3 a) { return dot(a, a); }

// Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
inline float3 srgb_to_linear(float3 c)
{
    return c * (c * (c * 0.305306011f + 0.682171111f) + 0.012522878f);
}
inline float3 linear_to_srgb(float3 c)
{
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    return (0.662002687f * sq1) + (0.684122060f * sq2) - (0.323583601f * sq3) - (0.0225411470f * c);
}
