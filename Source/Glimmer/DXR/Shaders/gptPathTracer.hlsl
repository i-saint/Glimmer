#include "gptMath.h"
#include "gptCommon.h"

RWTexture2D<float4>             g_frame_buffer  : register(u0, space0);
RWTexture2D<float4>             g_accum_buffer  : register(u1, space0);
RaytracingAccelerationStructure g_tlas          : register(t0, space0);
ConstantBuffer<SceneData>       g_scene         : register(b0, space0);

StructuredBuffer<InstanceData>  g_instances     : register(t0, space1);
StructuredBuffer<MeshData>      g_meshes        : register(t1, space1);
StructuredBuffer<MaterialData>  g_materials     : register(t2, space1);
SamplerState                    g_sampler_default : register(s0, space1);

StructuredBuffer<vertex_t>      g_vertices[]    : register(t0, space2);
StructuredBuffer<vertex_t>      g_vertices_d[]  : register(t0, space3);
StructuredBuffer<face_t>        g_faces[]       : register(t0, space4);
Texture2D<float4>               g_textures[]    : register(t0, space5);


float3 CameraPosition()     { return g_scene.camera.position.xyz; }
float3 CameraRight()        { return g_scene.camera.view[0].xyz; }
float3 CameraUp()           { return -g_scene.camera.view[1].xyz; }
float3 CameraForward()      { return g_scene.camera.view[2].xyz; }
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

uint InstanceFlags()        { return g_instances[InstanceID()].instance_flags; }
uint InstanceLayerMask()    { return g_instances[InstanceID()].layer_mask; }
int  MeshID()               { return g_instances[InstanceID()].mesh_id; }
int  DeformID()             { return g_instances[InstanceID()].deform_id; }

float3 FaceNormal(int instance_id, int face_id)
{
    int mesh_id = g_instances[instance_id].mesh_id;
    int deform_id = g_instances[instance_id].deform_id;
    int3 indices = g_faces[mesh_id][face_id].indices;

    float3 p0, p1, p2;
    if (deform_id != -1) {
        p0 = g_vertices_d[deform_id][indices[0]].position;
        p1 = g_vertices_d[deform_id][indices[1]].position;
        p2 = g_vertices_d[deform_id][indices[2]].position;
    }
    else {
        p0 = g_vertices[mesh_id][indices[0]].position;
        p1 = g_vertices[mesh_id][indices[1]].position;
        p2 = g_vertices[mesh_id][indices[2]].position;
    }
    return normalize(cross(p1 - p0, p2 - p0));
}
float3 FaceNormal()
{
    int instance_id = InstanceID();
    int face_id = PrimitiveIndex();
    return FaceNormal(instance_id, face_id);
}

MaterialData FaceMaterial(int instance_id, int face_id)
{
    int mesh_id = g_instances[instance_id].mesh_id;
    face_t face = g_faces[mesh_id][face_id];
    return g_materials[g_instances[instance_id].material_ids[face.material_index]];
}
MaterialData FaceMaterial()
{
    int instance_id = InstanceID();
    int face_id = PrimitiveIndex();
    return FaceMaterial(instance_id, face_id);
}

vertex_t HitVertex(float2 barycentric)
{
    int instance_id = InstanceID();
    int face_id = PrimitiveIndex();
    int mesh_id = g_instances[instance_id].mesh_id;
    int deform_id = g_instances[instance_id].deform_id;
    int3 indices = g_faces[mesh_id][face_id].indices;

    if (deform_id != -1) {
        return barycentric_interpolation(
            barycentric,
            g_vertices_d[deform_id][indices[0]],
            g_vertices_d[deform_id][indices[1]],
            g_vertices_d[deform_id][indices[2]]);
    }
    else {
        return barycentric_interpolation(
            barycentric,
            g_vertices[mesh_id][indices[0]],
            g_vertices[mesh_id][indices[1]],
            g_vertices[mesh_id][indices[2]]);
    }
}

float3 HitPosition()
{
    return offset_ray(WorldRayOrigin() + (WorldRayDirection() * RayTCurrent()), FaceNormal());
}

float3 GetDiffuseColor(MaterialData md, float2 uv)
{
    float3 r = md.diffuse;
    int tid = md.diffuse_tex;
    if (tid != -1)
        r *= g_textures[tid].SampleLevel(g_sampler_default, uv, 0).xyz;
    return r;
}

float3 GetEmissiveColor(MaterialData md, float2 uv)
{
    float3 r = md.emissive;
    int tid = md.emissive_tex;
    if (tid != -1)
        r += g_textures[tid].SampleLevel(g_sampler_default, uv, 0).xyz;
    return r;
}



struct RadiancePayload
{
    float3 radiance;
    float3 attenuation;
    float3 origin;
    float3 direction;
    uint   seed;
    int    done;
    int2   pad;

    void init()
    {
        radiance = 0.0f;
        attenuation = 1.0f;
        origin = 0.0f;
        direction = 0.0f;
        seed = 0;
        done = 0;
    }
};

struct OcclusionPayload
{
    int hit;

    void init()
    {
        hit = 0;
    }
};


void GetCameraRay(out float3 origin, out float3 direction, float2 offset = 0.0f)
{
    uint2 si = DispatchRaysIndex().xy;
    uint2 sd = DispatchRaysDimensions().xy;

    float aspect_ratio = (float)sd.x / (float)sd.y;
    float2 screen_pos = ((float2(si) + offset + 0.5f) / float2(sd)) * 2.0f - 1.0f;
    screen_pos.x *= aspect_ratio;

    origin = CameraPosition();
    direction = normalize(
        CameraRight() * screen_pos.x +
        CameraUp() * screen_pos.y +
        CameraForward() * CameraFocalLength());
}

void ShootRadianceRay(inout RadiancePayload payload)
{
    RayDesc ray;
    ray.Origin = payload.origin;
    ray.Direction = payload.direction;
    ray.TMin = CameraNearPlane(); // 
    ray.TMax = CameraFarPlane();  // todo: correct this

    uint render_flags = RenderFlags();
    uint ray_flags = 0;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(g_tlas, ray_flags, 0xff, RT_RADIANCE, 0, RT_RADIANCE, ray, payload);
}

[shader("raygeneration")]
void RayGenRadiance()
{
    uint2 si = DispatchRaysIndex().xy;
    uint2 sd = DispatchRaysDimensions().xy;

    float3 result = 0.0f;
    uint seed = tea(si.y * sd.x + si.x, FrameCount());
    int samples_per_frame = SamplesPerFrame();
    int max_trace_depth = MaxTraceDepth();
    for (int i = 0; i < samples_per_frame; ++i) {
        RadiancePayload payload;
        payload.init();
        payload.seed = seed;
        GetCameraRay(payload.origin, payload.direction, float2(rnd55(seed), rnd55(seed)));

        for (int d = 0; d < max_trace_depth && !payload.done; ++d) {
            ShootRadianceRay(payload);
            result += payload.radiance * payload.attenuation;
        }
    }

    float4 prev = g_accum_buffer[si];
    prev *= 0.9f;
    float accum = prev.w + float(samples_per_frame);
    result += prev.xyz;
    
    g_frame_buffer[si] = float4(result / accum, 0.0f);
    g_accum_buffer[si] = float4(result, accum);
}

[shader("miss")]
void MissRadiance(inout RadiancePayload payload : SV_RayRadiancePayload)
{
    payload.radiance = BackgroundColor();
    payload.done = true;
}


bool ShootOcclusionRay(uint flags, in RayDesc ray)
{
    OcclusionPayload payload;
    TraceRay(g_tlas, flags, 0xff, RT_OCCLUSION, 0, RT_OCCLUSION, ray, payload);
    return payload.hit;
}

[shader("closesthit")]
void ClosestHitRadiance(inout RadiancePayload payload : SV_RayRadiancePayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    float3 P = HitPosition();
    float3 N = FaceNormal();
    vertex_t v = HitVertex(attr.barycentrics);

    // prepare next trace
    {
        MaterialData md = FaceMaterial();
        uint seed = payload.seed;

        ONB onb;
        onb.init(N);
        float3 dir = cosine_sample_hemisphere(rnd01(seed), rnd01(seed));
        dir = onb.inverse_transform(dir);

        float3 ref = reflect(payload.direction, N);
        dir = normalize(lerp(ref, dir, md.roughness));

        payload.direction = dir;
        payload.origin = P;
        payload.attenuation *= GetDiffuseColor(md, v.uv);
        payload.radiance += GetEmissiveColor(md, v.uv);
    }


    // receive lights
    uint ray_flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_NON_OPAQUE;
    for (int li = 0; li < LightCount(); ++li) {

        float weight = 0.0f;
        const LightData light = GetLight(li);
        if (light.type == LT_DIRECTIONAL) {
            // directional light
            float3 L = -light.direction;

            RayDesc ray;
            ray.Origin = P;
            ray.Direction = L;
            ray.TMin = 0.0f;
            ray.TMax = CameraFarPlane();
            if (!ShootOcclusionRay(ray_flags, ray)) {
                float nDl = dot(N, L);
                weight = nDl;
            }
        }
        else if (light.type == LT_POINT) {
            // point light
            float3 L = normalize(light.position - P);
            float Ld = length(light.position - P);
            if (Ld <= light.range) {
                RayDesc ray;
                ray.Origin = P;
                ray.Direction = L;
                ray.TMin = 0.0f;
                ray.TMax = Ld;
                if (!ShootOcclusionRay(ray_flags, ray)) {
                    float nDl = dot(N, L);
                    //weight = nDl / (PI * Ld * Ld);

                    float a = (light.range - Ld) / light.range;
                    weight = nDl * (a * a);
                }
            }
        }
        else if (light.type == LT_SPOT) {
            // spot light
            float3 L = normalize(light.position - P);
            float Ld = length(light.position - P);
            if (Ld <= light.range && angle_between(-L, light.direction) * 2.0f <= light.spot_angle) {
                RayDesc ray;
                ray.Origin = P;
                ray.Direction = L;
                ray.TMin = 0.0f;
                ray.TMax = Ld;
                if (!ShootOcclusionRay(ray_flags, ray)) {
                    float nDl = dot(N, L);
                    //weight = nDl / (PI * Ld * Ld);

                    float a = (light.range - Ld) / light.range;
                    weight = nDl * (a * a);
                }
            }
        }

        payload.radiance += light.color * max(weight, 0.0f);
    }
}


[shader("miss")]
void MissOcclusion(inout OcclusionPayload payload : SV_RayPayload)
{
}

[shader("anyhit")]
void AnyHitOcclusion(inout OcclusionPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.hit = true;
}
