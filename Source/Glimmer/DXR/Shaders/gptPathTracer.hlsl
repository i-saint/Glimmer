#include "gptMath.h"
#include "gptCommon.h"

#define gptEnableStochasticLightCulling

enum RayType
{
    RT_RADIANCE,
    RT_OCCLUSION,
    RT_PHOTON,
};

RWTexture2D<float4>             g_rw_frame_buffer   : register(u0, space0);
RWTexture2D<float4>             g_rw_radiance_buffer: register(u1, space0);
RWTexture2D<float4>             g_rw_normal_buffer  : register(u2, space0);
RWTexture2D<float>              g_rw_depth_buffer   : register(u3, space0);
Texture2D<float4>               g_radiance_buffer   : register(t0, space0);
Texture2D<float4>               g_normal_buffer     : register(t1, space0);
Texture2D<float>                g_depth_buffer      : register(t2, space0);

RaytracingAccelerationStructure g_tlas          : register(t0, space1);
StructuredBuffer<LightData>     g_lights        : register(t1, space1);
ConstantBuffer<SceneData>       g_scene         : register(b0, space1);

StructuredBuffer<InstanceData>  g_instances     : register(t0, space2);
StructuredBuffer<MeshData>      g_meshes        : register(t1, space2);
StructuredBuffer<MaterialData>  g_materials     : register(t2, space2);

StructuredBuffer<int3>          g_indices[]     : register(t0, space3);
StructuredBuffer<vertex_t>      g_vertices[]    : register(t0, space4);
StructuredBuffer<vertex_t>      g_vertices_d[]  : register(t0, space5);
Texture2D<float4>               g_textures[]    : register(t0, space6);

SamplerState g_sampler_default : register(s0, space1);


uint  FrameCount()          { return g_scene.frame; }
uint  SamplesPerFrame()     { return g_scene.samples_per_frame; }
uint  MaxTraceDepth()       { return g_scene.max_trace_depth; }
float3 BackgroundColor()    { return g_scene.bg_color; }

float3 FaceNormal(int instance_id, int face_id)
{
    int mesh_id = g_instances[instance_id].mesh_id;
    int deform_id = g_instances[instance_id].deform_id;
    int3 indices = g_indices[mesh_id][face_id];

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
    return g_materials[g_instances[instance_id].material_id];
}
MaterialData FaceMaterial()
{
    int instance_id = InstanceID();
    int face_id = PrimitiveIndex();
    return FaceMaterial(instance_id, face_id);
}

float3 HitPosition()
{
    return WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
}

float3 InstancePosition(int instance_id)
{
    return g_instances[instance_id].transform[3].xyz;
}

vertex_t GetInterpolatedVertex(int instance_id, int face_id, float2 barycentric)
{
    int mesh_id = g_instances[instance_id].mesh_id;
    int deform_id = g_instances[instance_id].deform_id;
    int3 indices = g_indices[mesh_id][face_id];

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
vertex_t GetInterpolatedVertex(float2 barycentric)
{
    return GetInterpolatedVertex(InstanceID(), PrimitiveIndex(), barycentric);
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
        tn.xy *= -1.0f;
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
    uint   iteration;
    uint   done;

    void init()
    {
        radiance = 0.0f;
        attenuation = 1.0f;
        origin = 0.0f;
        direction = 0.0f;
        t = -1.0f;
        seed = 0;
        iteration = 0;
        done = false;
    }
};

struct OcclusionPayload
{
    float3 attenuation;
    float3 origin;
    float3 direction;
    int instance_id;
    int face_id;
    float2 barycentrics;
    uint   missed;

    void init()
    {
        attenuation = 1.0f;
        origin = 0.0f;
        direction = 0.0f;
        instance_id = -1;
        face_id = -1;
        barycentrics = 0.0f;
        missed = false;
    }
};


void GetCameraRay(out float3 origin, out float3 direction, CameraData cam, uint2 si, float2 offset)
{
    uint2 sd = g_scene.camera.screen_size;

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

    //// swirl effect
    //{
    //    float attenuation = max(1.0f - length(screen_pos), 0.0);
    //    float angle = (180.0f * sin(g_scene.time * 0.3f) * DegToRad) * (attenuation * attenuation);

    //    float3 axis= normalize(f * focal);
    //    float4 rot = rotate_axis(axis, angle);
    //    direction = mul(quat_to_mat33(rot), direction);
    //}
}

float3 GetCameraRayPosition(CameraData cam, uint2 si, float t)
{
    float3 origin, direction;
    GetCameraRay(origin, direction, cam, si, 0.0f);
    return origin + (direction * t);
}

void GetCameraRay(out float3 origin, out float3 direction, uint2 si, float2 offset = 0.0f)
{
    GetCameraRay(origin, direction, g_scene.camera, si, offset);
}

void ShootRadianceRay(inout RadiancePayload payload)
{
    RayDesc ray;
    ray.Origin = payload.origin;
    ray.Direction = payload.direction;
    ray.TMin = 0.0f;
    ray.TMax = g_scene.camera.far_plane;  // todo: correct this

    uint ray_flags = 0;
    //ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    TraceRay(g_tlas, ray_flags, LM_VISIBLE, RT_RADIANCE, 0, RT_RADIANCE, ray, payload);
}

int4 GetScreenIndex()
{
    int2 si = DispatchRaysIndex().xy;
    int2 sd = DispatchRaysDimensions().xy;
    int pi = sd.x * si.y + si.x;
    int bi = pi - WaveGetLaneIndex();
    return int4(si, pi, bi);
}

[shader("raygeneration")]
void RayGenRadiance()
{
    int4 si4 = GetScreenIndex();
    int2 si = si4.xy;
    int pi = si4.z;

    uint seed = tea(pi, FrameCount());
    int samples_per_frame = SamplesPerFrame();
    int max_trace_depth = MaxTraceDepth();
    float2 jitter = 0.0f;
    float3 radiance = 0.0f;
    float t = 0.0f;

    for (int i = 0; i < samples_per_frame; ++i) {
        RadiancePayload payload;
        payload.init();
        payload.seed = seed;
        GetCameraRay(payload.origin, payload.direction, si, jitter);

        for (int depth = 0; depth < max_trace_depth && !payload.done; ++depth) {
            payload.iteration = max_trace_depth * i + depth;
            ShootRadianceRay(payload);
            radiance += payload.radiance * payload.attenuation;
            if (i == 0 && depth == 0)
                t = payload.t;
        }
        seed = payload.seed;
        jitter = float2(rnd55(seed), rnd55(seed));
    }

    float accum = float(samples_per_frame);
    float3 prev_radiance = g_rw_radiance_buffer[si].xyz;
    float prev_accum = g_rw_radiance_buffer[si].w;
    float prev_t = g_rw_depth_buffer[si];
    float move_amount = 0;
    {
        float3 pos = GetCameraRayPosition(g_scene.camera, si, t);
        float3 pos_prev = GetCameraRayPosition(g_scene.camera_prev, si, prev_t);
        move_amount = length(pos - pos_prev);

        const float attenuation = max(0.95f - (move_amount * 100.0f), 0.0f);
        radiance += prev_radiance * attenuation;
        accum += prev_accum * attenuation;
    }

    if (t == g_scene.camera.far_plane) {
        g_rw_normal_buffer[si] = 0.0f;
    }
    g_rw_depth_buffer[si] = t;
    g_rw_radiance_buffer[si] = float4(radiance, accum);
}


[shader("raygeneration")]
void RayGenDisplay()
{
    int2 si = GetScreenIndex().xy;

    float3 n = normalize(g_normal_buffer[si].xyz);
    float d = g_depth_buffer[si].x;
    float4 radiance = g_radiance_buffer[si];

    g_rw_frame_buffer[si] = float4(linear_to_srgb(radiance.xyz / radiance.w), 1.0f);
    //g_rw_frame_buffer[si] = float4(n * 0.5f + 0.5f, 1.0f);
    //g_rw_frame_buffer[si] = float4(1.0f - d.xxx / g_scene.camera.far_plane, 1.0f);
    //g_rw_frame_buffer[si] = (float)WaveGetLaneIndex() / (float)WaveGetLaneCount();
}


[shader("miss")]
void MissRadiance(inout RadiancePayload payload : SV_RayRadiancePayload)
{
    payload.radiance = BackgroundColor();
    payload.done = true;
    payload.t = g_scene.camera.far_plane;
}


bool TraceOcclusion(in RayDesc ray, out float3 direction, out float3 attenuation)
{
    OcclusionPayload payload;
    payload.init();
    payload.origin = ray.Origin;
    payload.direction = ray.Direction;

    //uint ray_flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    // todo: repeat this if !payload.occluded
    uint ray_flags = 0;
    TraceRay(g_tlas, ray_flags, LM_SHADOW, RT_OCCLUSION, 0, RT_OCCLUSION, ray, payload);
    attenuation = payload.attenuation;
    direction = payload.direction;
    return payload.instance_id != -1;
}

OcclusionPayload TraceEmissive(in RayDesc ray)
{
    OcclusionPayload payload;
    payload.init();
    payload.origin = ray.Origin;
    payload.direction = ray.Direction;

    uint ray_flags = 0;
    TraceRay(g_tlas, ray_flags, LM_SHADOW | LM_LIGHT_SOURCE, RT_OCCLUSION, 0, RT_OCCLUSION, ray, payload);
    return payload;
}

float GetLightContribution(float3 P, float3 N, int light_index)
{
    const LightData light = g_lights[light_index];
    if (light.type == LT_DIRECTIONAL) {
        // directional light
        return light.intensity;
    }
    else if (light.type == LT_POINT) {
        // point light
        float3 L = normalize(light.position - P);
        float Ld = length(light.position - P);
        if (Ld <= light.range) {
            float a = (light.range - Ld) / light.range;
            float weight = (a * a);
            return light.intensity * weight;
        }
    }
    else if (light.type == LT_SPOT) {
        // spot light
        float3 L = normalize(light.position - P);
        float Ld = length(light.position - P);
        if (Ld <= light.range && angle_between(-L, light.direction) * 2.0f <= light.spot_angle) {

            float a = (light.range - Ld) / light.range;
            float weight = (a * a);
            return light.intensity * weight;
        }
    }
    else if (light.type == LT_MESH) {
        // mesh light
        int ii = light.mesh_instance_id;
        if (ii == InstanceID())
            return 0.0f; // already accumerated in ClosestHitRadiance()

        float3 Lpos = InstancePosition(ii);
        float Ld = length(Lpos - P);
        float a = (light.range - Ld) / light.range;
        float weight = (a * a);
        return light.intensity * weight;
    }

    return 0.0f;
}

void PickLight(float3 P, float3 N, inout uint seed, out int light_index, out float contribution)
{
    light_index = -1;
    contribution = 0.0f;

    int li;
    float total_contribution = 0.0f;
    for (li = 0; li < g_scene.light_count; ++li)
        total_contribution += GetLightContribution(P, N, li);

    float p = rnd01(seed) * total_contribution;
    for (li = 0; li < g_scene.light_count; ++li) {
        float c = GetLightContribution(P, N, li);
        p -= c;
        if (p <= 0.0f) {
            light_index = li;
            contribution = c / total_contribution;
            break;
        }
    }
}

float3 GetLightRadiance(float3 P, float3 N, int light_index, inout uint seed)
{
    float3 radiance = 0.0f;

    const LightData light = g_lights[light_index];
    if (light.type == LT_DIRECTIONAL) {
        // directional light
        float3 L = -light.direction;
        L = normalize(L + (rnd_dir(seed) * light.disperse));

        RayDesc ray;
        ray.Origin = P;
        ray.Direction = L;
        ray.TMin = 0.0f;
        ray.TMax = g_scene.camera.far_plane;

        float3 attenuation;
        if (!TraceOcclusion(ray, L, attenuation)) {
            float nDl = dot(N, L);
            float weight = nDl;
            radiance = (light.color * light.intensity) * attenuation * max(weight, 0.0f);
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

            float3 attenuation;
            if (!TraceOcclusion(ray, L, attenuation)) {
                float nDl = dot(N, L);
                //weight = nDl / (PI * Ld * Ld);

                float a = (light.range - Ld) / light.range;
                float weight = nDl * (a * a);
                radiance = (light.color * light.intensity) * attenuation * max(weight, 0.0f);
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

            float3 attenuation;
            if (!TraceOcclusion(ray, L, attenuation)) {
                float nDl = dot(N, L);
                //weight = nDl / (PI * Ld * Ld);

                float a = (light.range - Ld) / light.range;
                float weight = nDl * (a * a);
                radiance = (light.color * light.intensity) * attenuation * max(weight, 0.0f);
            }
        }
    }
    else if (light.type == LT_MESH) {
        // mesh light
        int ii = light.mesh_instance_id;
        if (ii == InstanceID())
            return 0.0f; // already accumerated in ClosestHitRadiance()

        int mesh_id = g_instances[ii].mesh_id;
        MeshData mesh = g_meshes[mesh_id];
        MaterialData md = g_materials[g_instances[ii].material_id];

        int fid = rnd_i(seed, mesh.triangle_count);
        float2 bc = rnd_bc(seed);
        float3 fpos = GetInterpolatedVertex(ii, fid, bc).position;
        float3 L = normalize(fpos - P);
        float Ld = length(fpos - P);
        if (Ld >= light.range)
            return 0.0f;

        RayDesc ray;
        ray.Origin = P;
        ray.Direction = L;
        ray.TMin = 0.0f;
        ray.TMax = Ld + 0.01f; // todo: improve offset

        OcclusionPayload epl = TraceEmissive(ray);
        if (epl.instance_id == -1) { // hit transparent face
            epl.instance_id = ii;
            epl.face_id = fid;
            epl.barycentrics = bc;
        }
        if (epl.instance_id == ii) {
            vertex_t hv = GetInterpolatedVertex(epl.instance_id, epl.face_id, epl.barycentrics);
            float Ld = length(hv.position - P);
            float nDl = dot(N, ray.Direction);
            float a = (light.range - Ld) / light.range;
            float weight = max(nDl, 0.0f) * (a * a);

            radiance += GetEmissive(md, hv.uv) * epl.attenuation * (light.intensity * weight);
        }
    }
    return radiance;
}

inline bool Refract(inout float3 pos, inout float3 dir, float3 vertex_normal, float3 face_normal, float refraction_index, bool backface)
{
    // assume refraction index of air is 1.0f
    float3 rdir = backface ?
        refract(dir, -vertex_normal, refraction_index) :
        refract(dir, vertex_normal, 1.0f / refraction_index);

    if (length_sq(rdir) == 0.0f) {
        // perfect reflection
        dir = reflect(rdir, vertex_normal);
        pos = offset_ray(pos, backface ? -face_normal : face_normal);
        return false;
    }
    else {
        dir = rdir;
        pos = offset_ray(pos, backface ? face_normal : -face_normal);
        return true;
    }
}

[shader("closesthit")]
void ClosestHitRadiance(inout RadiancePayload payload : SV_RayRadiancePayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    MaterialData md = FaceMaterial();
    vertex_t V = GetInterpolatedVertex(attr.barycentrics);

    bool backface = HitKind() == HIT_KIND_TRIANGLE_BACK_FACE;
    float3 Nf = FaceNormal();
    float3 N = GetNormal(V, md);

    float3 P_ = HitPosition();
    float3 P = offset_ray(P_, Nf);
    uint seed = payload.seed;
    payload.t = RayTCurrent();

    {
        // prepare next ray

        if (md.type == MT_TRANSPARENT) {
            payload.origin = P_;
            Refract(payload.origin, payload.direction, N, Nf, md.refraction_index, backface);

            if (backface) {
                payload.attenuation *= GetDiffuse(md, V.uv) * ((1.0f - md.opacity) / (1.0f + payload.t * payload.t));
            }
            else {
                //// diffuse & emissive
                //payload.attenuation *= GetDiffuse(md, V.uv);
                //payload.radiance += GetEmissive(md, V.uv);
            }
        }
        else {
            float3 reflect_dir = reflect(payload.direction, N);
            float3 diffuse_dir = onb_inverse_transform(cosine_sample_hemisphere(rnd01(seed), rnd01(seed)), N);
            payload.direction = normalize(lerp(reflect_dir, diffuse_dir, GetRoughness(md, V.uv)));
            payload.origin = P;

            // diffuse & emissive
            payload.attenuation *= GetDiffuse(md, V.uv);
            payload.radiance += GetEmissive(md, V.uv);
        }
    }

    float3 radiance = 0.0f;

#ifdef gptEnableStochasticLightCulling
    // stochastic light culling
    int light_index;
    float light_contribution;
    PickLight(P, N, seed, light_index, light_contribution);
    if (light_index != -1)
        radiance += GetLightRadiance(P, N, light_index, seed) / light_contribution;
#else
    // enumerate all light
    for (int li = 0; li < g_scene.light_count; ++li)
        radiance += GetLightRadiance(P, N, li, seed);
#endif


    payload.radiance += radiance * md.opacity;
    payload.seed = seed;

    if (payload.iteration == 0) {
        g_rw_normal_buffer[GetScreenIndex().xy] = float4(N, 0.0f);
    }
}


[shader("miss")]
void MissOcclusion(inout OcclusionPayload payload : SV_RayPayload)
{
    payload.missed = true;
}

[shader("closesthit")]
void ClosestHitOcclusion(inout OcclusionPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    MaterialData md = FaceMaterial();
    if (md.type == MT_TRANSPARENT) {
        vertex_t V = GetInterpolatedVertex(attr.barycentrics);
        float3 Nf = FaceNormal();
        float3 N = GetNormal(V, md);
        bool backface = HitKind() == HIT_KIND_TRIANGLE_BACK_FACE;

        payload.origin = HitPosition();
        Refract(payload.origin, payload.direction, N, Nf, md.refraction_index, backface);

        if (!backface) {
            payload.attenuation *= GetDiffuse(md, V.uv) * (1.0f - md.opacity);
        }
    }
    else {
        payload.instance_id = InstanceID();
        payload.face_id = PrimitiveIndex();
        payload.barycentrics = attr.barycentrics;
    }
}



// for photon pass
RWTexture2D<float4>      g_photon_buffer      : register(u0, space10);

struct PhotonPayload
{
    float3 attenuation;
    float3 origin;
    float3 direction;
    int instance_id;
    int face_id;
    float2 barycentrics;
    uint   missed;

    void init()
    {
        attenuation = 1.0f;
        origin = 0.0f;
        direction = 0.0f;
        instance_id = -1;
        face_id = -1;
        barycentrics = 0.0f;
        missed = false;
    }
};

struct photon_t
{
    float3 radiance;
    float3 position;
};

vertex_t PickRandomVertex(int instance_id, inout uint seed)
{
    MeshData mesh = g_meshes[g_instances[instance_id].mesh_id];
    int fid = rnd_i(seed, mesh.triangle_count);
    float2 bc = rnd_bc(seed);
    return GetInterpolatedVertex(instance_id, fid, bc);
}


PhotonPayload TracePhoton(float3 pos, float3 dir, float range)
{
    RayDesc ray;
    ray.Origin = pos;
    ray.Direction = dir;
    ray.TMin = 0.0f;
    ray.TMax = range;

    PhotonPayload payload;
    payload.init();
    payload.origin = pos;
    payload.direction = dir;

    //uint ray_flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    // todo: repeat this if !payload.occluded
    uint ray_flags = 0;
    TraceRay(g_tlas, ray_flags, LM_SHADOW, RT_PHOTON, 0, RT_PHOTON, ray, payload);
    return payload;
}

photon_t GetLightPhoton(int light_index, inout uint seed)
{
    vertex_t V = PickRandomVertex(0, seed); // todo:

    float3 position = 0.0f;
    float3 radiance = 0.0f;

    const LightData light = g_lights[light_index];
    if (light.type == LT_DIRECTIONAL) {
        // directional light
        float3 P = V.position + (light.direction * 100.f); // todo: fix this
        float3 L = light.direction;
        L = normalize(L + (rnd_dir(seed) * light.disperse));

        PhotonPayload ppl = TracePhoton(P, L, g_scene.camera.far_plane);
        if (ppl.instance_id != -1) {
            vertex_t Vh = GetInterpolatedVertex(ppl.instance_id, ppl.face_id, ppl.barycentrics);
            float3 N = Vh.normal;

            float nDl = dot(N, L);
            float weight = nDl;
            radiance = (light.color * light.intensity) * ppl.attenuation * max(weight, 0.0f);
        }
    }
    else if (light.type == LT_POINT) {
        // point light
        float3 P = light.position;
        float Ld = length(V.position - P);
        float3 L = normalize(V.position - P);
        L = normalize(L + (rnd_dir(seed) * light.disperse));
        if (Ld <= light.range) {

            PhotonPayload ppl = TracePhoton(P, L, light.range);
            if (ppl.instance_id != -1) {
                vertex_t Vh = GetInterpolatedVertex(ppl.instance_id, ppl.face_id, ppl.barycentrics);
                float3 N = Vh.normal;

                float nDl = dot(N, L);
                //weight = nDl / (PI * Ld * Ld);

                float a = (light.range - Ld) / light.range;
                float weight = nDl * (a * a);
                radiance = (light.color * light.intensity) * ppl.attenuation * max(weight, 0.0f);
            }
        }
    }
    else if (light.type == LT_SPOT) {
        // spot light
        float3 P = light.position;
        float Ld = length(V.position - P);
        float3 L = normalize(V.position - P);
        L = normalize(L + (rnd_dir(seed) * light.disperse));
        if (Ld <= light.range && angle_between(-L, light.direction) * 2.0f <= light.spot_angle) {

            PhotonPayload ppl = TracePhoton(P, L, light.range);
            if (ppl.instance_id != -1) {
                vertex_t Vh = GetInterpolatedVertex(ppl.instance_id, ppl.face_id, ppl.barycentrics);
                float3 N = Vh.normal;

                float nDl = dot(N, L);
                //weight = nDl / (PI * Ld * Ld);

                float a = (light.range - Ld) / light.range;
                float weight = nDl * (a * a);
                radiance = (light.color * light.intensity) * ppl.attenuation * max(weight, 0.0f);
            }
        }
    }
    else if (light.type == LT_MESH) {
        int ii = light.mesh_instance_id;
        int mesh_id = g_instances[ii].mesh_id;
        MeshData mesh = g_meshes[mesh_id];
        MaterialData md = g_materials[g_instances[ii].material_id];

        vertex_t Vl = PickRandomVertex(ii, seed);

        vertex_t V = PickRandomVertex(0, seed); // todo: pick random vertex

        RayDesc ray;
        ray.Origin = Vl.position;
        ray.Direction = normalize(V.position - Vl.position);
        ray.TMin = 0.0f;
        ray.TMax = light.range + 0.01f; // todo: improve offset

        PhotonPayload ppl;
        ppl.init();
        ppl.origin = ray.Origin;
        ppl.direction = ray.Direction;

        uint ray_flags = 0;
        TraceRay(g_tlas, ray_flags, LM_SHADOW | LM_LIGHT_SOURCE, RT_OCCLUSION, 0, RT_OCCLUSION, ray, ppl);

        if (ppl.instance_id != -1) {
            vertex_t Vh = GetInterpolatedVertex(ppl.instance_id, ppl.face_id, ppl.barycentrics);
            float Ld = length(Vh.position - Vl.position);
            float nDl = dot(Vh.normal, ray.Direction);
            float a = (light.range - Ld) / light.range;
            float weight = max(nDl, 0.0f) * (a * a);

            radiance = GetEmissive(md, Vh.uv) * ppl.attenuation * (light.intensity * weight);
            position = Vh.position;
        }
    }

    photon_t r;
    r.position = position;
    r.radiance = radiance;
    return r;
}


[shader("raygeneration")]
void RayGenPhotonPass1()
{
    // just clear buffer
    g_photon_buffer[DispatchRaysIndex().xy] = 0.0f;
}

[shader("raygeneration")]
void RayGenPhotonPass2()
{
    uint si = DispatchRaysIndex().x;
    uint seed = tea(si, FrameCount());
    int samples_per_frame = SamplesPerFrame();
    int max_trace_depth = MaxTraceDepth();

    // receive lights
    photon_t photon;
    for (int li = 0; li < g_scene.light_count; ++li) {
        photon = GetLightPhoton(li, seed);
        // todo: store in g_photon_buffer
    }
}

[shader("miss")]
void MissPhoton(inout PhotonPayload payload : SV_RayPayload)
{
    payload.missed = true;
}

[shader("closesthit")]
void ClosestHitPhoton(inout PhotonPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    MaterialData md = FaceMaterial();
    if (md.type == MT_TRANSPARENT) {
        vertex_t V = GetInterpolatedVertex(attr.barycentrics);
        float3 Nf = FaceNormal();
        float3 N = GetNormal(V, md);
        bool backface = HitKind() == HIT_KIND_TRIANGLE_BACK_FACE;

        payload.origin = HitPosition();
        Refract(payload.origin, payload.direction, N, Nf, md.refraction_index, backface);

        if (!backface) {
            payload.attenuation *= GetDiffuse(md, V.uv) * (1.0f - md.opacity);
        }
    }
    else {
        payload.origin = HitPosition();
        payload.instance_id = InstanceID();
        payload.face_id = PrimitiveIndex();
        payload.barycentrics = attr.barycentrics;
    }
}
