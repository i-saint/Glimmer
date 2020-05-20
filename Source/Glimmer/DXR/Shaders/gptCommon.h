#pragma once

#define kMaxLights 32

enum LightType
{
    LT_DIRECTIONAL,
    LT_POINT,
    LT_SPOT,
    LT_MESH,
};

enum MaterialType
{
    MT_OPAQUE,
    MT_ALPHA_TO_COVERAGE,
    MT_TRANSPARENT,
    MT_TRANSLUCENT,
};

enum RenderFlag
{
    RF_CULL_BACK_FACES = 0x00000001,
};

enum MeshFlag
{
    MF_HAS_BLENDSHAPES  = 0x00000001,
    MF_HAS_JOINTS       = 0x00000002,
    MF_IS_DYNAMIC       = 0x00000004,
};

enum LayerMask
{
    LM_VISIBLE      = 0x00000001,
    LM_SHADOW       = 0x00000002,
    LM_LIGHT_SOURCE = 0x00000004,
};


struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
    float3 position;
    float fov;
    float4 rotation;
    float near_plane;
    float far_plane;
    int2  screen_size;
};

struct LightData
{
    int type; // LightType
    float3 position;
    float3 direction;
    float range;
    float3 color;
    float intensity;
    float spot_angle; // radian
    float disperse;
    int mesh_instance_id; // for mesh light
    int pad;
};

struct MaterialData
{
    int type; // MaterialType
    float3 diffuse;
    float3 emissive;
    float roughness;
    float opacity;
    float refraction_index;
    int diffuse_tex;
    int opacity_tex;
    int roughness_tex;
    int emissive_tex;
    int normal_tex;
    int pad;
};

struct BlendshapeData
{
    int frame_count;
    int frame_offset;
};
struct BlendshapeFrameData
{
    float weight;
    int delta_offset;
};
struct JointCount
{
    int weight_count;
    int weight_offset;
};
struct JointWeight
{
    float weight;
    int joint_index;
};
struct MeshData
{
    int vertex_count;
    int triangle_count;
    int deform_id;
    int flags;
};

struct InstanceData
{
    float4x4 transform;
    float4x4 itransform;
    int mesh_id;
    int deform_id;
    int material_id;
    int triangle_count;
    int triangle_offset;
    int instance_flags;
    int2 pad;
};

struct SceneData
{
    int frame;
    int samples_per_frame;
    int max_trace_depth;
    int light_count;
    int meshlight_count;
    float3 bg_color;
    float time;
    float3 pad;

    CameraData camera;
    CameraData camera_prev;
};

struct vertex_t
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
    float pad;

    void clear()
    {
        position = 0.0f;
        normal = 0.0f;
        tangent = 0.0f;
        uv = 0.0f;
        pad = 0.0f;
    }
};

struct accum_t
{
    float3 radiance;
    float accum;
    float t;

    float3 pad;
};



#include "gptMath.h"

inline vertex_t barycentric_interpolation(float2 barycentric, vertex_t p0, vertex_t p1, vertex_t p2)
{
    vertex_t r;
    r.position  = barycentric_interpolation(barycentric, p0.position, p1.position, p2.position);
    r.normal    = barycentric_interpolation(barycentric, p0.normal, p1.normal, p2.normal);
    r.tangent   = barycentric_interpolation(barycentric, p0.tangent, p1.tangent, p2.tangent);
    r.uv        = barycentric_interpolation(barycentric, p0.uv, p1.uv, p2.uv);
    return r;
}
