#define kMaxLights 32

enum LightType
{
    LT_DIRECTIONAL,
    LT_SPOT,
    LT_POINT,
    LT_REVERSE_POINT,
};

enum MaterialType
{
    MT_DEFAULT,
};

enum RayType
{
    RT_RADIANCE,
    RT_OCCLUSION,
};

enum RenderFlag
{
    RF_CULL_BACK_FACES      = 0x00000001,
    RF_FLIP_CASTER_FACES    = 0x00000002,
    RF_IGNORE_SELF_SHADOW   = 0x00000004,
    RF_KEEP_SELF_DROP_SHADOW= 0x00000008,
};

enum InstanceFlag
{
    IF_RECEIVE_SHADOWS  = 0x01,
    IF_SHADOWS_ONLY     = 0x02,
    IF_CAST_SHADOWS     = 0x04,
};

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4 position;
    float4 rotation;
    float near_plane;
    float far_plane;
    float fov;
    float pad;
};

struct LightData
{
    uint type; // LightType
    float3 position;
    float3 direction;
    float range;
    float3 color;
    float spot_angle; // radian
};

struct MaterialData
{
    uint type; // MaterialType
    float3 diffuse;
    float3 emissive;
    float roughness;
    float opacity;
    int diffuse_tex;
    int emissive_tex;
    float pad;
};

struct MeshData
{
    uint face_count;
    uint index_count;
    uint vertex_count;
    uint mesh_flags;
};

struct InstanceData
{
    float4x4 local_to_world;
    float4x4 world_to_local;
    uint mesh_index;
    uint material_index;
    uint instance_flags;
    uint layer_mask;
};

struct SceneData
{
    uint render_flags;
    uint sample_count;
    uint light_count;
    uint pad1;
    float3 bg_color;
    float pad2;

    CameraData camera;
    LightData lights[kMaxLights];
};

struct vertex_t
{
    float3 point_;
    float3 normal;
    float3 tangent;
    float2 uv;
    float pad;
};

struct face_t
{
    int3 indices;
    float3 normal;
    float2 pad;
};


RWTexture2D<float4>             g_frame_buffer  : register(u0, space0);
RWTexture2D<float4>             g_accum_buffer  : register(u1, space0);
RaytracingAccelerationStructure g_tlas          : register(t0, space0);
Texture2D<float4>               g_prev_buffer   : register(t1, space0);
ConstantBuffer<SceneData>       g_scene         : register(b0, space0);

StructuredBuffer<InstanceData>  g_instances     : register(t0, space1);
StructuredBuffer<MeshData>      g_meshes        : register(t1, space1);
StructuredBuffer<MaterialData>  g_materials     : register(t2, space1);
StructuredBuffer<vertex_t>      g_vertices[]    : register(t0, space2);
StructuredBuffer<face_t>        g_faces[]       : register(t0, space3);
Texture2D<float4>               g_textures[]    : register(t0, space4);


float3 CameraPosition()     { return g_scene.camera.position.xyz; }
float3 CameraRight()        { return g_scene.camera.view[0].xyz; }
float3 CameraUp()           { return g_scene.camera.view[1].xyz; }
float3 CameraForward()      { return -g_scene.camera.view[2].xyz; }
float CameraFocalLength()   { return abs(g_scene.camera.proj[1][1]); }
float CameraNearPlane()     { return g_scene.camera.near_plane; }
float CameraFarPlane()      { return g_scene.camera.far_plane; }

uint  RenderFlags()         { return g_scene.render_flags; }
uint  SampleCount()         { return g_scene.sample_count; }
float3 BackgroundColor()    { return g_scene.bg_color; }

int LightCount() { return g_scene.light_count; }
LightData GetLight(int i) { return g_scene.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - 0.01f); }

uint InstanceFlags() { return g_instances[InstanceID()].instance_flags; }
uint InstanceLayerMask() { return g_instances[InstanceID()].layer_mask; }

// a & b must be normalized
float angle_between(float3 a, float3 b) { return acos(clamp(dot(a, b), 0, 1)); }

// "A Fast and Robust Method for Avoiding Self-Intersection":
// http://www.realtimerendering.com/raytracinggems/unofficial_RayTracingGems_v1.5.pdf
float3 offset_ray(const float3 p, const float3 n)
{
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float int_scale = 256.0f;

    int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);
    float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));
    return float3(
        abs(p.x) < origin ? p.x + float_scale*n.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale*n.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale*n.z : p_i.z);
}


struct Payload
{
    float3 color;
    float t;
};

void init(inout Payload a)
{
    a.color = 0.0f;
    a.t = -1.0f;
}

RayDesc GetCameraRay(float2 offset = 0.0f)
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 screen_dim = DispatchRaysDimensions().xy;

    float aspect_ratio = (float)screen_dim.x / (float)screen_dim.y;
    float2 screen_pos = ((float2(screen_idx) + offset + 0.5f) / float2(screen_dim)) * 2.0f - 1.0f;
    screen_pos.x *= aspect_ratio;

    RayDesc ray;
    ray.Origin = CameraPosition();
    ray.Direction = normalize(
        CameraRight() * screen_pos.x +
        CameraUp() * screen_pos.y +
        CameraForward() * CameraFocalLength());
    ray.TMin = CameraNearPlane(); // 
    ray.TMax = CameraFarPlane();  // todo: correct this
    return ray;
}

Payload ShootRadianceRay(float2 offset = 0.0f)
{
    Payload payload;
    init(payload);

    RayDesc ray = GetCameraRay(offset);
    uint render_flags = RenderFlags();
    uint ray_flags = 0;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(g_tlas, ray_flags, 0xff, RT_RADIANCE, 0, RT_RADIANCE, ray, payload);
    return payload;
}

float SampleDifferentialF(int2 idx, out float center, out float diff)
{
    int2 dim;
    g_prev_buffer.GetDimensions(dim.x, dim.y);

    center = g_prev_buffer[idx].x;
    diff = 0;

    // 4 samples for now. an option for more samples may be needed. it can make an unignorable difference (both quality and speed).
    diff += abs(g_prev_buffer[clamp(idx + int2(-1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_buffer[clamp(idx + int2( 1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_buffer[clamp(idx + int2( 0,-1), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_buffer[clamp(idx + int2( 0, 1), int2(0, 0), dim - 1)].x - center);
    return diff;
}

uint SampleDifferentialI(int2 idx, out uint center, out uint diff)
{
    int2 dim;
    g_prev_buffer.GetDimensions(dim.x, dim.y);

    center = asuint(g_prev_buffer[idx].x);
    diff = 0;

    // 4 samples for now. an option for more samples may be needed. it can make an unignorable difference (both quality and speed).
    diff += abs(asuint(g_prev_buffer[clamp(idx + int2(-1, 0), int2(0, 0), dim - 1)].x) - center);
    diff += abs(asuint(g_prev_buffer[clamp(idx + int2( 1, 0), int2(0, 0), dim - 1)].x) - center);
    diff += abs(asuint(g_prev_buffer[clamp(idx + int2( 0,-1), int2(0, 0), dim - 1)].x) - center);
    diff += abs(asuint(g_prev_buffer[clamp(idx + int2( 0, 1), int2(0, 0), dim - 1)].x) - center);
    return diff;
}

[shader("raygeneration")]
void RayGenDefault()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    Payload payload = ShootRadianceRay();
    g_frame_buffer[screen_idx] = float4(payload.color, payload.t);
}

[shader("miss")]
void MissRadiance(inout Payload payload : SV_RayPayload)
{
    payload.color = BackgroundColor();
}

bool ShootOcclusionRay(uint flags, in RayDesc ray, inout Payload payload)
{
    TraceRay(g_tlas, flags, 0xff, RT_OCCLUSION, 0, RT_OCCLUSION, ray, payload);
    return payload.t >= 0.0f;
}

[shader("closesthit")]
void ClosestHitRadiance(inout Payload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    // shoot shadow ray (hit position -> light)

    uint render_flags = RenderFlags();
    uint ray_flags = 0;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    int li;
    for (li = 0; li < LightCount(); ++li) {
        LightData light = GetLight(li);

        bool hit = true;
        if (light.type == LT_DIRECTIONAL) {
            // directional light
            RayDesc ray;
            ray.Origin = HitPosition();
            ray.Direction = -light.direction.xyz;
            ray.TMin = 0.0f;
            ray.TMax = CameraFarPlane();
            hit = ShootOcclusionRay(ray_flags, ray, payload);
        }
        else if (light.type == LT_SPOT) {
            // spot light
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);
            if (distance <= light.range && angle_between(-dir, light.direction) * 2.0f <= light.spot_angle) {
                RayDesc ray;
                ray.Origin = pos;
                ray.Direction = dir;
                ray.TMin = 0.0f;
                ray.TMax = distance;
                hit = ShootOcclusionRay(ray_flags, ray, payload);
            }
            else
                continue;
        }
        else if (light.type == LT_POINT) {
            // point light
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);

            if (distance <= light.range) {
                RayDesc ray;
                ray.Origin = pos;
                ray.Direction = dir;
                ray.TMin = 0.0f;
                ray.TMax = distance;
                hit = ShootOcclusionRay(ray_flags, ray, payload);
            }
            else
                continue;
        }
        else if (light.type == LT_REVERSE_POINT) {
            // reverse point light
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);

            if (distance <= light.range) {
                RayDesc ray;
                ray.Origin = pos;
                ray.Direction = -dir;
                ray.TMin = 0.0f;
                ray.TMax = light.range - distance;
                hit = ShootOcclusionRay(ray_flags, ray, payload);
            }
            else
                continue;
        }

        if (!hit) {
            payload.color += light.color;
        }
    }
}


[shader("miss")]
void MissOcclusion(inout Payload payload : SV_RayPayload)
{
    payload.color = float3(0.4f, 0.4f, 0.4f);
}

[shader("closesthit")]
void ClosestHitOcclusion(inout Payload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.t = RayTCurrent();
}
