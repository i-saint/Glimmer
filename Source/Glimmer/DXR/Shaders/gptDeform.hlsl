#include "gptCommon.h"

#define kThreadBlockSize 4

// per-instance data
RWStructuredBuffer<vertex_t>            g_dst_vertices  : register(u0, space0);
StructuredBuffer<float4x4>              g_joint_matrices: register(t0, space0);
StructuredBuffer<float>                 g_bs_weights    : register(t1, space0);

// per-mesh data
StructuredBuffer<vertex_t>              g_src_vertices  : register(t0, space1);
StructuredBuffer<JointCount>            g_joint_counts  : register(t1, space1);
StructuredBuffer<JointWeight>           g_joint_weights : register(t2, space1);
StructuredBuffer<BlendshapeData>        g_bs            : register(t3, space1);
StructuredBuffer<BlendshapeFrameData>   g_bs_frames     : register(t4, space1);
StructuredBuffer<vertex_t>              g_bs_delta      : register(t5, space1);
StructuredBuffer<MeshData>              g_mesh          : register(t6, space1);


uint VertexCount()
{
    uint n, s;
    g_dst_vertices.GetDimensions(n, s);
    return n;
}

uint BlendshapeCount()
{
    uint n, s;
    g_bs.GetDimensions(n, s);
    return n;
}

uint DeformFlags()
{
    return g_mesh[0].flags;
}


float GetBlendshapeWeight(uint bsi)
{
    return g_bs_weights[bsi];
}

uint GetBlendshapeFrameCount(uint bsi)
{
    return g_bs[bsi].frame_count;
}

float GetBlendshapeFrameWeight(uint bsi, uint fi)
{
    uint offset = g_bs[bsi].frame_offset;
    return g_bs_frames[offset + fi].weight;
}

vertex_t GetBlendshapeDelta(uint bsi, uint fi, uint vi)
{
    uint offset = g_bs_frames[g_bs[bsi].frame_offset + fi].delta_offset;
    return g_bs_delta[offset + vi];
}


uint GetVertexJointCount(uint vi)
{
    return g_joint_counts[vi].weight_count;
}

float GetVertexJointWeight(uint vi, uint ji)
{
    int offset = g_joint_counts[vi].weight_offset;
    return g_joint_weights[ji + offset].weight;
}

float4x4 GetVertexJointMatrix(uint vi, uint ji)
{
    int offset = g_joint_counts[vi].weight_offset;
    uint i = g_joint_weights[ji + offset].joint_index;
    return g_joint_matrices[i];
}

vertex_t lerp(vertex_t a, vertex_t b, float c)
{
    vertex_t r;
    r.position  = lerp(a.position, b.position, c);
    r.normal    = lerp(a.normal,   b.normal,   c);
    r.tangent   = lerp(a.tangent,  b.tangent,  c);
    r.uv        = lerp(a.uv,       b.uv,       c);
    return r;
}

vertex_t add(vertex_t a, vertex_t b)
{
    vertex_t r;
    r.position  = a.position + b.position;
    r.normal    = a.normal   + b.normal;
    r.tangent   = a.tangent  + b.tangent;
    r.uv        = a.uv       + b.uv;
    return r;
}

vertex_t madd(vertex_t a, vertex_t b, float c)
{
    vertex_t r;
    r.position  = a.position + (b.position * c);
    r.normal    = a.normal   + (b.normal   * c);
    r.tangent   = a.tangent  + (b.tangent  * c);
    r.uv        = a.uv       + (b.uv       * c);
    return r;
}

void ApplyBlendshape(uint vi, inout vertex_t v)
{
    vertex_t result = v;

    uint blendshape_count = BlendshapeCount();
    for (uint bsi = 0; bsi < blendshape_count; ++bsi) {
        float weight = GetBlendshapeWeight(bsi);
        if (weight == 0.0f)
            continue;

        uint frame_count = GetBlendshapeFrameCount(bsi);
        float last_weight = GetBlendshapeFrameWeight(bsi, frame_count - 1);

        if (weight < 0.0f) {
            vertex_t delta = GetBlendshapeDelta(bsi, 0, vi);
            float s = weight / GetBlendshapeFrameWeight(bsi, 0);
            result = madd(result, delta, s);
        }
        else if (weight > last_weight) {
            vertex_t delta = GetBlendshapeDelta(bsi, frame_count - 1, vi);
            float s = 0.0f;
            if (frame_count >= 2) {
                float prev_weight = GetBlendshapeFrameWeight(bsi, frame_count - 2);
                s = (weight - prev_weight) / (last_weight - prev_weight);
            }
            else {
                s = weight / last_weight;
            }
            result = madd(result, delta, s);
        }
        else {
            float w1 = 0.0f, w2 = 0.0f;
            vertex_t p1, p2;
            p1.clear();
            p2.clear();

            for (uint fi = 0; fi < frame_count; ++fi) {
                float frame_weight = GetBlendshapeFrameWeight(bsi, fi);
                if (weight <= frame_weight) {
                    p2 = GetBlendshapeDelta(bsi, fi, vi);
                    w2 = frame_weight;
                    break;
                }
                else {
                    p1 = GetBlendshapeDelta(bsi, fi, vi);
                    w1 = frame_weight;
                }
            }
            float s = (weight - w1) / (w2 - w1);
            result = add(result, lerp(p1, p2, s));
        }
    }
    result.normal = normalize(result.normal);
    result.tangent = normalize(result.tangent);
    v = result;
}

void ApplySkinning(uint vi, inout vertex_t v)
{
    float4 pos_base = float4(v.position, 1.0f);
    float4 normal_base = float4(v.normal, 0.0f);
    float4 tangent_base = float4(v.tangent, 0.0f);
    float3 pos_deformed = 0.0f;
    float3 normal_deformed = 0.0f;
    float3 tangent_deformed = 0.0f;

    uint joint_count = GetVertexJointCount(vi);
    for (uint ji = 0; ji < joint_count; ++ji) {
        float w = GetVertexJointWeight(vi, ji);
        float4x4 m = GetVertexJointMatrix(vi, ji);
        pos_deformed += mul(m, pos_base).xyz * w;
        normal_deformed += mul(m, normal_base).xyz * w;
        tangent_deformed += mul(m, tangent_base).xyz * w;
    }
    v.position = pos_deformed;
    v.normal = normalize(normal_deformed);
    v.tangent = normalize(tangent_deformed);
}

[numthreads(kThreadBlockSize, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint vi = tid.x;

    vertex_t v = g_src_vertices[vi];
    uint deform_flags = DeformFlags();
    if (deform_flags & MF_HAS_BLENDSHAPES)
        ApplyBlendshape(vi, v);
    if (deform_flags & MF_HAS_JOINTS)
        ApplySkinning(vi, v);

    g_dst_vertices[vi] = v;
}
