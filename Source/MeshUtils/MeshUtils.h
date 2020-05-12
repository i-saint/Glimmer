#pragma once
#define MeshUtils_h

#include <vector>
#include <memory>
#include "muRawVector.h"
#include "muHalf.h"
#include "muMath.h"
#include "muQuat32.h"
#include "muLimits.h"
#include "muSIMD.h"
#include "muAlgorithm.h"
#include "muImage.h"
#include "muTLS.h"
#include "muMisc.h"
#include "muTime.h"
#include "muConcurrency.h"
#include "muCompression.h"
#include "muStream.h"

namespace mu {

struct MeshConnectionInfo;

bool GenerateTriangleFaceNormals(RawVector<float3>& dst, const Span<float3> points, const Span<int> indices, bool flip);

bool GenerateNormalsPoly(RawVector<float3>& dst,
    const Span<float3> points, const Span<int> counts, const Span<int> indices, bool flip);

void GenerateNormalsWithSmoothAngle(RawVector<float3>& dst,
    const Span<float3> points,
    const Span<int> counts, const Span<int> indices,
    float smooth_angle, bool flip);


// PointsIter: indexed_iterator<const float3*, int*> or indexed_iterator_s<const float3*, int*>
template<class PointsIter>
void GenerateNormalsPoly(float3 *dst,
    PointsIter vertices, const int *counts, const int *offsets, const int *indices,
    int num_faces, int num_vertices);

// PointsIter: indexed_iterator<const float3*, int*> or indexed_iterator_s<const float3*, int*>
// UVIter: indexed_iterator<const float2*, int*> or indexed_iterator_s<const float2*, int*>
template<class PointsIter, class UVIter>
void GenerateTangentsPoly(float4 *dst,
    PointsIter vertices, UVIter uv, const float3 *normals,
    const int *counts, const int *offsets, const int *indices,
    int num_faces, int num_vertices);

void QuadifyTriangles(const Span<float3> vertices, const Span<int> triangle_indices, bool full_search, float threshold_angle,
    RawVector<int>& dst_indices, RawVector<int>& dst_counts);

template<class Handler>
void SelectEdge(const Span<int>& indices, int ngon, const Span<float3>& vertices,
    const Span<int>& vertex_indices, const Handler& handler);
template<class Handler>
void SelectEdge(const Span<int>& indices, const Span<int>& counts, const Span<int>& offsets, const Span<float3>& vertices,
    const Span<int>& vertex_indices, const Handler& handler);

template<class Handler>
void SelectHole(const Span<int>& indices, int ngon, const Span<float3>& vertices,
    const Span<int>& vertex_indices, const Handler& handler);
template<class Handler>
void SelectHole(const Span<int>& indices, const Span<int>& counts, const Span<int>& offsets, const Span<float3>& vertices,
    const Span<int>& vertex_indices, const Handler& handler);

template<class Handler>
void SelectConnected(const Span<int>& indices, int ngon, const Span<float3>& vertices,
    const Span<int>& vertex_indices, const Handler& handler);
template<class Handler>
void SelectConnected(const Span<int>& indices, const Span<int>& counts, const Span<int>& offsets, const Span<float3>& vertices,
    const Span<int>& vertex_indices, const Handler& handler);


// ------------------------------------------------------------
// impl
// ------------------------------------------------------------

// Body: [](int face_index, int vertex_index) -> void
template<class Indices, class Body>
inline void EnumerateFaceIndices(const Indices& counts, const Body& body)
{
    int num_faces = (int)counts.size();
    int i = 0;
    for (int fi = 0; fi < num_faces; ++fi) {
        int count = counts[fi];
        for (int ci = 0; ci < count; ++ci)
            body(fi, i + ci);
        i += count;
    }
}

// Body: [](int face_index, int vertex_index, int reverse_vertex_index) -> void
template<class Indices, class Body>
inline void EnumerateReverseFaceIndices(const Indices& counts, const Body& body)
{
    int num_faces = (int)counts.size();
    int i = 0;
    for (int fi = 0; fi < num_faces; ++fi) {
        int count = counts[fi];
        for (int ci = 0; ci < count; ++ci) {
            int index = i + ci;
            int rindex = i + (count - ci - 1);
            body(fi, index, rindex);
        }
        i += count;
    }
}

template<class T, class Indices>
inline void CopyWithIndices(T *dst, const T *src, const Indices& indices, size_t beg, size_t end)
{
    if (!dst || !src)
        return;

    size_t size = end - beg;
    for (size_t i = 0; i < size; ++i)
        dst[i] = src[indices[beg + i]];
}

template<class T, class Indices>
inline void CopyWithIndices(T *dst, const T *src, const Indices& indices)
{
    if (!dst || !src)
        return;

    size_t n = indices.size();
    for (size_t i = 0; i < n; ++i)
        dst[i] = src[indices[i]];
}

template<class Counts, class Offsets>
inline void CountIndices(
    const Counts& counts, Offsets& offsets,
    int& num_indices, int& num_indices_triangulated)
{
    int reti = 0, rett = 0;
    size_t num_faces = counts.size();
    offsets.resize(num_faces);
    for (size_t fi = 0; fi < num_faces; ++fi)
    {
        auto f = counts[fi];
        offsets[fi] = reti;
        reti += f;
        rett += std::max<int>(f - 2, 0) * 3;
    }
    num_indices = reti;
    num_indices_triangulated = rett;
}


inline void MirrorPoints(float3 *dst, size_t n, float3 plane_n, float plane_d)
{
    if (!dst)
        return;
    for (size_t i = 0; i < n; ++i)
        dst[i] = plane_mirror(dst[i], plane_n, plane_d);
}

inline void MirrorVectors(float3 *dst, size_t n, float3 plane_n)
{
    if (!dst)
        return;
    for (size_t i = 0; i < n; ++i)
        dst[i] = plane_mirror(dst[i], plane_n);
}
inline void MirrorVectors(float4 *dst, size_t n, float3 plane_n)
{
    if (!dst)
        return;
    for (size_t i = 0; i < n; ++i)
        (float3&)dst[i] = plane_mirror((float3&)dst[i], plane_n);
}

inline void MirrorTopology(int *dst_counts, int *dst_indices, const Span<int>& counts, const Span<int>& indices, int offset)
{
    if (!dst_counts || !dst_indices)
        return;

    memcpy(dst_counts, counts.data(), sizeof(int) * counts.size());
    EnumerateReverseFaceIndices(counts, [&](int, int idx, int ridx) {
        dst_indices[idx] = offset + indices[ridx];
    });
}

inline void MirrorTopology(int *dst_counts, int *dst_indices, const Span<int>& counts, const Span<int>& indices, const Span<int>& indirect)
{
    if (!dst_counts || !dst_indices)
        return;

    memcpy(dst_counts, counts.data(), sizeof(int) * counts.size());
    EnumerateReverseFaceIndices(counts, [&](int, int idx, int ridx) {
        dst_indices[idx] = indirect[indices[ridx]];
    });
}

} // namespace mu

#include "MeshUtils_impl.h"
#include "muMeshRefiner.h"
