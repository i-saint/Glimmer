#include "gptMath.h"
#include "gptCommon.h"

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

uint  FrameCount()          { return g_scene.frame; }
uint  SamplesPerFrame()     { return g_scene.samples_per_frame; }
uint  MaxTraceDepth()       { return g_scene.max_trace_depth; }
uint  RenderFlags()         { return g_scene.render_flags; }
float3 BackgroundColor()    { return g_scene.bg_color; }

int LightCount()            { return g_scene.light_count; }
LightData GetLight(int i)   { return g_scene.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - 0.01f); }

uint InstanceFlags() { return g_instances[InstanceID()].instance_flags; }
uint InstanceLayerMask() { return g_instances[InstanceID()].layer_mask; }


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

[shader("raygeneration")]
void RayGenRadiance()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 screen_dim = DispatchRaysDimensions().xy;

    Payload payload = ShootRadianceRay();
    g_frame_buffer[screen_idx] = float4(payload.color, payload.t);

    //uint seed = tea(screen_idx.y * screen_dim.x + screen_idx.x, FrameCount());
    //g_frame_buffer[screen_idx] = float4(
    //    rnd(seed) - 0.5f,
    //    rnd(seed) - 0.5f,
    //    rnd(seed) - 0.5f,
    //    rnd(seed) - 0.5f
    //);
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
    uint ray_flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
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