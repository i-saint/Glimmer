#define kMaxLights 32

enum LIGHT_TYPE
{
    LT_DIRECTIONAL,
    LT_SPOT,
    LT_POINT,
    LT_REVERSE_POINT,
};

enum RENDER_FLAG
{
    RF_CULL_BACK_FACES      = 0x00000001,
    RF_FLIP_CASTER_FACES    = 0x00000002,
    RF_IGNORE_SELF_SHADOW   = 0x00000004,
    RF_KEEP_SELF_DROP_SHADOW= 0x00000008,
};

enum INSTANCE_FLAG
{
    IF_RECEIVE_SHADOWS  = 0x01,
    IF_SHADOWS_ONLY     = 0x02,
    IF_CAST_SHADOWS     = 0x04,
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
    uint light_type;
    float3 position;
    float3 direction;
    float range;
    float3 color;
    float spot_angle; // radian
};

struct MaterialData
{
    float3 diffuse;
    float roughness;
    float3 emissive;
    float opacity;
    int diffuse_tex;
    int emissive_tex;
    int2 pad_tex;
};

struct MeshData
{
    uint face_offset;
    uint face_count;
    uint index_offset;
    uint index_count;
    uint vertex_offset;
    uint vertex_count;
    uint mesh_flags;
    uint pad;
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
    uint light_count;
    float shadow_ray_offset;
    float self_shadow_threshold;
    float3 pad;

    CameraData camera;
    LightData lights[kMaxLights];
};


// slot 0
RWTexture2D<float> g_output : register(u0);
RaytracingAccelerationStructure g_tlas : register(t0);
ConstantBuffer<SceneData> g_scene : register(b0);

// slot 1
StructuredBuffer<InstanceData> g_instances : register(t1);
StructuredBuffer<MaterialData> g_materials : register(t2);
StructuredBuffer<MeshData>     g_meshes : register(t3);
StructuredBuffer<vertex_t>     g_vertices : register(t4);
StructuredBuffer<face_t>       g_faces: register(t5);

// slot 2
Texture2D<float> g_prev_result : register(t6);


float3 CameraPosition()     { return g_scene.camera.position.xyz; }
float3 CameraRight()        { return g_scene.camera.view[0].xyz; }
float3 CameraUp()           { return g_scene.camera.view[1].xyz; }
float3 CameraForward()      { return -g_scene.camera.view[2].xyz; }
float CameraFocalLength()   { return abs(g_scene.camera.proj[1][1]); }
float CameraNearPlane()     { return g_scene.camera.near_plane; }
float CameraFarPlane()      { return g_scene.camera.far_plane; }

uint  RenderFlags()         { return g_scene.render_flags; }
float ShadowRayOffset()     { return g_scene.shadow_ray_offset; }
float SelfShadowThreshold() { return g_scene.self_shadow_threshold; }

int LightCount() { return g_scene.light_count; }
LightData GetLight(int i) { return g_scene.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - ShadowRayOffset()); }

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


struct CameraPayload
{
    float shadow;
    uint light_bits;
    float t;
    uint instance_id; // instance id for first ray
};
struct LightPayload
{
    uint hit;
    uint instance_id; // instance id for first ray
};

void init(inout CameraPayload a)
{
    a.shadow = 0.0;
    a.light_bits = 0;
    a.t = 1e+100;
}
void init(inout LightPayload a, in CameraPayload b)
{
    a.hit = ~0;
    a.instance_id = b.instance_id;
}
void select(inout CameraPayload a, in CameraPayload b)
{
    if (b.t < a.t)
        a = b;
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

CameraPayload ShootCameraRay(float2 offset = 0.0f)
{
    CameraPayload payload;
    init(payload);

    RayDesc ray = GetCameraRay(offset);
    uint render_flags = RenderFlags();
    uint ray_flags = 0;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(g_tlas, ray_flags, 0x01, 0, 0, 0, ray, payload);
    return payload;
}

float SampleDifferentialF(int2 idx, out float center, out float diff)
{
    int2 dim;
    g_prev_result.GetDimensions(dim.x, dim.y);

    center = g_prev_result[idx].x;
    diff = 0;

    // 4 samples for now. an option for more samples may be needed. it can make an unignorable difference (both quality and speed).
    diff += abs(g_prev_result[clamp(idx + int2(-1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_result[clamp(idx + int2( 1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_result[clamp(idx + int2( 0,-1), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_result[clamp(idx + int2( 0, 1), int2(0, 0), dim - 1)].x - center);
    return diff;
}

uint SampleDifferentialI(int2 idx, out uint center, out uint diff)
{
    int2 dim;
    g_prev_result.GetDimensions(dim.x, dim.y);

    center = asuint(g_prev_result[idx].x);
    diff = 0;

    // 4 samples for now. an option for more samples may be needed. it can make an unignorable difference (both quality and speed).
    diff += abs(asuint(g_prev_result[clamp(idx + int2(-1, 0), int2(0, 0), dim - 1)].x) - center);
    diff += abs(asuint(g_prev_result[clamp(idx + int2( 1, 0), int2(0, 0), dim - 1)].x) - center);
    diff += abs(asuint(g_prev_result[clamp(idx + int2( 0,-1), int2(0, 0), dim - 1)].x) - center);
    diff += abs(asuint(g_prev_result[clamp(idx + int2( 0, 1), int2(0, 0), dim - 1)].x) - center);
    return diff;
}

[shader("raygeneration")]
void RayGenDefault()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    CameraPayload payload = ShootCameraRay();
    g_output[screen_idx] = payload.shadow;
}

[shader("raygeneration")]
void RayGenAdaptiveSampling()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 cur_dim = DispatchRaysDimensions().xy;
    uint2 pre_dim; g_prev_result.GetDimensions(pre_dim.x, pre_dim.y);
    int2 pre_idx = (int2)((float2)screen_idx * ((float2)pre_dim / (float2)cur_dim));

    float center, diff;
    SampleDifferentialF(pre_idx, center, diff);
    g_output[screen_idx] = diff == 0 ? center : ShootCameraRay().shadow;
}

[shader("raygeneration")]
void RayGenAntialiasing()
{
    uint2 idx = DispatchRaysIndex().xy;

    float center, diff;
    SampleDifferentialF(idx, center, diff);

    if (diff == 0) {
        g_output[idx] = center;
    }
    else {
        // todo: make offset values shader parameter
        const int N = 4;
        const float d = 0.333f;
        float2 offsets[N] = {
            float2(d, d), float2(-d, d), float2(-d, -d), float2(d, -d)
        };

        float total = asfloat(center);
        for (int i = 0; i < N; ++i)
            total += ShootCameraRay(offsets[i]).shadow;
        g_output[idx] = total / (float)(N + 1);
    }
}


[shader("miss")]
void MissCamera(inout CameraPayload payload : SV_RayPayload)
{
    // nothing todo here
}

[shader("anyhit")]
void AnyHitCamera(inout CameraPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
}

bool ShootShadowRay(uint flags, in RayDesc ray, inout CameraPayload payload)
{
    LightPayload lp;
    init(lp, payload);
    TraceRay(g_tlas, flags, 0x02, 1, 0, 1, ray, lp);
    return lp.hit;
}

[shader("closesthit")]
void ClosestHitCamera(inout CameraPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.t = RayTCurrent();
    payload.instance_id = InstanceID();

    // shoot shadow ray (hit position -> light)

    uint instance_flags = InstanceFlags();
    uint instance_layer_mask = InstanceLayerMask();
    float ls = 1.0f / LightCount();

    uint render_flags = RenderFlags();
    uint ray_flags_common = 0;
    if (render_flags & RF_CULL_BACK_FACES) {
        if (render_flags & RF_FLIP_CASTER_FACES)
            ray_flags_common |= RAY_FLAG_CULL_FRONT_FACING_TRIANGLES;
        else
            ray_flags_common |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    }
    if (render_flags & RF_IGNORE_SELF_SHADOW)
        ray_flags_common |= RAY_FLAG_FORCE_NON_OPAQUE; // use anyhit
    else
        ray_flags_common |= RAY_FLAG_FORCE_OPAQUE & RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    int li;
    for (li = 0; li < LightCount(); ++li) {
        LightData light = GetLight(li);
        uint ray_flags = ray_flags_common;

        bool hit = true;
        if (light.light_type == LT_DIRECTIONAL) {
            // directional light
            RayDesc ray;
            ray.Origin = HitPosition();
            ray.Direction = -light.direction.xyz;
            ray.TMin = 0.0f;
            ray.TMax = CameraFarPlane();
            hit = ShootShadowRay(ray_flags, ray, payload);
        }
        else if (light.light_type == LT_SPOT) {
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
                hit = ShootShadowRay(ray_flags, ray, payload);
            }
            else
                continue;
        }
        else if (light.light_type == LT_POINT) {
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
                hit = ShootShadowRay(ray_flags, ray, payload);
            }
            else
                continue;
        }
        else if (light.light_type == LT_REVERSE_POINT) {
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
                hit = ShootShadowRay(ray_flags, ray, payload);
            }
            else
                continue;
        }

        if (!hit) {
            payload.light_bits |= 0x1 << li;
            payload.shadow += ls;
        }
    }
}


[shader("miss")]
void MissLight(inout LightPayload payload : SV_RayPayload)
{
    payload.hit = 0;
}

[shader("anyhit")]
void AnyHitLight(inout LightPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    uint render_flags = RenderFlags();
    if (render_flags & RF_IGNORE_SELF_SHADOW) {
        // this condition means:
        // - always ignore zero-distance shadow
        //   (comparing instance ID is not enough because in some cases meshes are separated but seamlessly continuous. e.g. head and body)
        // - always ignore shadow cast by self back faces (relevanet only when 'cull back faces' is disabled)
        // - ignore non-zero-distance self shadow if 'keep self drop shadow' is disabled
        if (RayTCurrent() < SelfShadowThreshold() ||
            (payload.instance_id == InstanceID() && ((RenderFlags() & RF_KEEP_SELF_DROP_SHADOW) == 0 || HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)))
        {
            IgnoreHit();
            return;
        }
    }

    AcceptHitAndEndSearch();
}
