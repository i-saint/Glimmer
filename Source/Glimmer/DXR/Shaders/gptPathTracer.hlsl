#include "gptMath.h"
#include "gptCommon.h"

enum RayType
{
    RT_RADIANCE,
    RT_OCCLUSION,
    RT_EMISSIVE,
};

RWTexture2D<float4>             g_frame_buffer  : register(u0, space0);
RWStructuredBuffer<accum_t>     g_accum_buffer  : register(u1, space0);
RaytracingAccelerationStructure g_tlas          : register(t0, space0);
ConstantBuffer<SceneData>       g_scene         : register(b0, space0);

StructuredBuffer<InstanceData>  g_instances     : register(t0, space1);
StructuredBuffer<MeshData>      g_meshes        : register(t1, space1);
StructuredBuffer<MaterialData>  g_materials     : register(t2, space1);

StructuredBuffer<vertex_t>      g_vertices[]    : register(t0, space2);
StructuredBuffer<vertex_t>      g_vertices_d[]  : register(t0, space3);
StructuredBuffer<face_t>        g_faces[]       : register(t0, space4);
Texture2D<float4>               g_textures[]    : register(t0, space5);

SamplerState g_sampler_default : register(s0, space1);


uint  FrameCount()          { return g_scene.frame; }
uint  SamplesPerFrame()     { return g_scene.samples_per_frame; }
uint  MaxTraceDepth()       { return g_scene.max_trace_depth; }
float3 BackgroundColor()    { return g_scene.bg_color; }

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
    float3 n = normalize(cross(p1 - p0, p2 - p0));
    return mul_v(g_instances[instance_id].transform, n);
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

vertex_t HitVertex(int instance_id, int face_id, float2 barycentric)
{
    int mesh_id = g_instances[instance_id].mesh_id;
    int deform_id = g_instances[instance_id].deform_id;
    int3 indices = g_faces[mesh_id][face_id].indices;

    vertex_t r;
    if (deform_id != -1) {
        r = barycentric_interpolation(
            barycentric,
            g_vertices_d[deform_id][indices[0]],
            g_vertices_d[deform_id][indices[1]],
            g_vertices_d[deform_id][indices[2]]);
    }
    else {
        r = barycentric_interpolation(
            barycentric,
            g_vertices[mesh_id][indices[0]],
            g_vertices[mesh_id][indices[1]],
            g_vertices[mesh_id][indices[2]]);
    }
    //if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
    //    r.normal *= -1.0f;

    float4x4 transform = g_instances[instance_id].transform;
    r.position = mul_p(transform, r.position);
    r.normal = normalize(mul_v(transform, r.normal));
    r.tangent = normalize(mul_v(transform, r.tangent));
    return r;
}
vertex_t HitVertex(float2 barycentric)
{
    return HitVertex(InstanceID(), PrimitiveIndex(), barycentric);
}

float3 HitPosition()
{
    float3 pos = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
    return offset_ray(pos, FaceNormal());
}

float3 GetDiffuse(MaterialData md, float2 uv)
{
    float3 r = md.diffuse;
    int tid = md.diffuse_tex;
    if (tid != -1)
        r *= g_textures[tid].SampleLevel(g_sampler_default, uv, 0).xyz;
    return r;
}

float3 GetEmissive(MaterialData md, float2 uv)
{
    float3 r = md.emissive;
    int tid = md.emissive_tex;
    if (tid != -1)
        r += g_textures[tid].SampleLevel(g_sampler_default, uv, 0).xyz;
    return r;
}

float3 GetRoughness(MaterialData md, float2 uv)
{
    float3 r = md.roughness;
    int tid = md.roughness_tex;
    if (tid != -1)
        r *= g_textures[tid].SampleLevel(g_sampler_default, uv, 0).x;
    return r;
}

float3 GetNormal(vertex_t V, MaterialData md)
{
    float3 normal = V.normal;
    int tid = md.normal_tex;
    if (tid != -1) {
        float3 tangent = V.tangent;
        float3 binormal = normalize(cross(normal, tangent));
        float3x3 tbn = float3x3(tangent, binormal, normal);

        float3 tn = g_textures[tid].SampleLevel(g_sampler_default, V.uv, 0).xyz * 2.0f - 1.0f;
        normal = mul(tbn, tn);
    }
    return normal;
}



struct RadiancePayload
{
    float3 radiance;
    float3 attenuation;
    float3 origin;
    float3 direction;
    float  t;
    uint   seed;
    uint   done;
    uint   pad;

    void init()
    {
        radiance = 0.0f;
        attenuation = 1.0f;
        origin = 0.0f;
        direction = 0.0f;
        t = -1.0f;
        seed = 0;
        done = false;
    }
};

struct OcclusionPayload
{
    int hit;

    void init()
    {
        hit = false;
    }
};

struct EmissivePayload
{
    int instance_id;
    int face_id;
    float2 barycentrics;

    void init()
    {
        instance_id = -1;
        face_id = -1;
        barycentrics = 0.0f;
    }
};


void GetCameraRay(out float3 origin, out float3 direction, CameraData cam, float2 offset)
{
    uint2 si = DispatchRaysIndex().xy;
    uint2 sd = DispatchRaysDimensions().xy;

    float aspect_ratio = (float)sd.x / (float)sd.y;
    float2 screen_pos = ((float2(si) + offset + 0.5f) / float2(sd)) * 2.0f - 1.0f;
    screen_pos.x *= aspect_ratio;

    origin = cam.position;
    float3 r = cam.view[0].xyz;
    float3 u = -cam.view[1].xyz;
    float3 f = cam.view[2].xyz;
    float focal = abs(cam.proj[1][1]);

    direction = normalize(
        r * screen_pos.x +
        u * screen_pos.y +
        f * focal);
}

float3 GetCameraRayPosition(CameraData cam, float t)
{
    float3 origin, direction;
    GetCameraRay(origin, direction, cam, 0.0f);
    return origin + (direction * t);
}

void GetCameraRay(out float3 origin, out float3 direction, float2 offset = 0.0f)
{
    GetCameraRay(origin, direction, g_scene.camera, offset);
}

void ShootRadianceRay(inout RadiancePayload payload)
{
    RayDesc ray;
    ray.Origin = payload.origin;
    ray.Direction = payload.direction;
    ray.TMin = g_scene.camera.near_plane; // 
    ray.TMax = g_scene.camera.far_plane;  // todo: correct this

    uint ray_flags = 0;
    TraceRay(g_tlas, ray_flags, 0xff, RT_RADIANCE, 0, RT_RADIANCE, ray, payload);
}

[shader("raygeneration")]
void RayGenRadiance()
{
    uint2 si = DispatchRaysIndex().xy;
    uint2 sd = DispatchRaysDimensions().xy;
    uint si1 = sd.x * si.y + si.x;

    uint seed = tea(si.y * sd.x + si.x, FrameCount());
    int samples_per_frame = SamplesPerFrame();
    int max_trace_depth = MaxTraceDepth();
    float2 jitter = 0.0f;
    float3 radiance = 0.0f;
    float t = 0.0f;

    for (int i = 0; i < samples_per_frame; ++i) {
        RadiancePayload payload;
        payload.init();
        payload.seed = seed;
        GetCameraRay(payload.origin, payload.direction, jitter);

        for (int depth = 0; depth < max_trace_depth && !payload.done; ++depth) {
            ShootRadianceRay(payload);
            radiance += payload.radiance * payload.attenuation;
            if (i == 0 && depth == 0)
                t = payload.t;
        }
        seed = payload.seed;
        jitter = float2(rnd55(seed), rnd55(seed));
    }

    float accum = float(samples_per_frame);
    accum_t prev = g_accum_buffer[si1];
    float move_amount = 0;
    {
        float3 pos = GetCameraRayPosition(g_scene.camera, t);
        float3 pos_prev = GetCameraRayPosition(g_scene.camera_prev, prev.t);
        move_amount = length(pos - pos_prev);

        const float attenuation = max(0.95f - (move_amount * 100.0f), 0.0f);
        radiance += prev.radiance * attenuation;
        accum += prev.accum * attenuation;
    }
    prev.radiance = radiance;
    prev.accum = accum;
    prev.t = t;

    g_frame_buffer[si] = float4(radiance / accum, 0.0f);
    //g_frame_buffer[si] = d * 100.0f;
    g_accum_buffer[si1] = prev;
}

[shader("miss")]
void MissRadiance(inout RadiancePayload payload : SV_RayRadiancePayload)
{
    payload.radiance = BackgroundColor();
    payload.done = true;
    payload.t = g_scene.camera.far_plane;
}


bool ShootOcclusionRay(in RayDesc ray)
{
    OcclusionPayload payload;
    payload.init();

    uint ray_flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    TraceRay(g_tlas, ray_flags, 0xff, RT_OCCLUSION, 0, RT_OCCLUSION, ray, payload);
    return payload.hit;
}

EmissivePayload ShootEmissiveRay(in RayDesc ray)
{
    EmissivePayload payload;
    payload.init();

    uint ray_flags = 0;
    TraceRay(g_tlas, ray_flags, 0xff, RT_EMISSIVE, 0, RT_EMISSIVE, ray, payload);
    return payload;
}

[shader("closesthit")]
void ClosestHitRadiance(inout RadiancePayload payload : SV_RayRadiancePayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    MaterialData md = FaceMaterial();
    vertex_t V = HitVertex(attr.barycentrics);
    float3 P = HitPosition();
    float3 N = GetNormal(V, md);
    uint seed = payload.seed;
    payload.t = RayTCurrent();

    const bool enable_mesh_light = true;

    {
        // prepare next ray
        ONB onb;
        onb.init(N);
        float3 dir = cosine_sample_hemisphere(rnd01(seed), rnd01(seed));
        dir = onb.inverse_transform(dir);

        float3 ref = reflect(payload.direction, N);
        dir = normalize(lerp(ref, dir, GetRoughness(md, V.uv)));

        payload.direction = dir;
        payload.origin = P;

        // handle diffuse & emissive
        payload.attenuation *= GetDiffuse(md, V.uv);
        payload.radiance += GetEmissive(md, V.uv);
    }


    // receive lights
    for (int li = 0; li < g_scene.light_count; ++li) {

        float weight = 0.0f;
        const LightData light = g_scene.lights[li];
        if (light.type == LT_DIRECTIONAL) {
            // directional light
            float3 L = -light.direction;
            L = normalize(L + (rnd_dir(seed) * light.disperse));

            RayDesc ray;
            ray.Origin = P;
            ray.Direction = L;
            ray.TMin = 0.0f;
            ray.TMax = g_scene.camera.far_plane;
            if (!ShootOcclusionRay(ray)) {
                float nDl = dot(N, L);
                weight = nDl;
            }
        }
        else if (light.type == LT_POINT) {
            // point light
            float3 L = normalize(light.position - P);
            L = normalize(L + (rnd_dir(seed) * light.disperse));
            float Ld = length(light.position - P);
            if (Ld <= light.range) {
                RayDesc ray;
                ray.Origin = P;
                ray.Direction = L;
                ray.TMin = 0.0f;
                ray.TMax = Ld;
                if (!ShootOcclusionRay(ray)) {
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
            L = normalize(L + (rnd_dir(seed) * light.disperse));
            float Ld = length(light.position - P);
            if (Ld <= light.range && angle_between(-L, light.direction) * 2.0f <= light.spot_angle) {
                RayDesc ray;
                ray.Origin = P;
                ray.Direction = L;
                ray.TMin = 0.0f;
                ray.TMax = Ld;
                if (!ShootOcclusionRay(ray)) {
                    float nDl = dot(N, L);
                    //weight = nDl / (PI * Ld * Ld);

                    float a = (light.range - Ld) / light.range;
                    weight = nDl * (a * a);
                }
            }
        }

        payload.radiance += (light.color * light.intensity) * max(weight, 0.0f);
    }


    if (enable_mesh_light) {
        for (int ii = 0; ii < g_scene.instance_count; ++ii) {
            if (!g_instances[ii].enabled || ii == InstanceID())
                continue;

            MaterialData md = g_materials[g_instances[ii].material_ids[0]];
            if (dot(md.emissive, md.emissive) != 0 || md.emissive_tex != -1) {
                int mesh_id = g_instances[ii].mesh_id;
                MeshData mesh = g_meshes[mesh_id];
                int face = (int)((float)mesh.face_count * rnd01(seed));
                float3 fpos = HitVertex(ii, face, float2(rnd01(seed), rnd01(seed))).position;
                float distance = length(fpos - P);
                if (distance >= md.emissive_range)
                    continue;

                RayDesc ray;
                ray.Origin = P;
                ray.Direction = normalize(fpos - P);
                ray.TMin = 0.0f;
                ray.TMax = distance + 0.01f;

                EmissivePayload epl = ShootEmissiveRay(ray);
                if (epl.instance_id == ii) {
                    vertex_t hv = HitVertex(epl.instance_id, epl.face_id, epl.barycentrics);

                    float Ld = length(hv.position - P);
                    float nDl = dot(N, ray.Direction);
                    float a = (md.emissive_range - Ld) / md.emissive_range;
                    float weight = max(nDl, 0.0f) * (a * a);

                    payload.radiance += GetEmissive(md, hv.uv) * weight;
                }
            }
        }
    }

    payload.seed = seed;
}


[shader("miss")]
void MissOcclusion(inout OcclusionPayload payload : SV_RayPayload)
{
}

[shader("closesthit")]
void ClosestHitOcclusion(inout OcclusionPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.hit = true;
}


[shader("miss")]
void MissEmissive(inout EmissivePayload payload : SV_RayPayload)
{
}

[shader("closesthit")]
void ClosestHitEmissive(inout EmissivePayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.instance_id = InstanceID();
    payload.face_id = PrimitiveIndex();
    payload.barycentrics = attr.barycentrics;
}
