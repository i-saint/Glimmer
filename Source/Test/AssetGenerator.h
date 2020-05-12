#pragma once
#include "MeshUtils/MeshUtils.h"
using mu::float2;
using mu::float3;
using mu::float4;
using mu::float3x3;
using mu::float4x4;
using mu::quatf;
using mu::unorm8x4;
using mu::half4;
using mu::int2;


// T: unorm8x4, half4, float4
template<class T>
void MakeCheckerImage(T* pixels, int width, int height, int block_size);

template<class T>
void MakePolkaDotImage(T* pixels, int width, int height, int block_size);

template<class T>
void MakeNormalMapFromHeightMap(T* dst, const T* src, int width, int height);

void MakeCubeMesh(
    RawVector<int>& indices,
    RawVector<float3>& points,
    RawVector<float3>& normals,
    RawVector<float2>& uv,
    float size);

void MakeTorusMesh(
    RawVector<int>& indices,
    RawVector<float3>& points,
    RawVector<float3>& normals,
    RawVector<float2>& uv,
    float inner_radius, float outer_radius,
    int div1, int div2);


void MakeIcoSphereMesh(
    RawVector<int>& indices,
    RawVector<float3>& points,
    RawVector<float3>& normals,
    float radius,
    int iteration);

void MakeWaveMesh(
    RawVector<int>& indices,
    RawVector<float3> &points,
    float size, float height,
    const int resolution,
    float angle);

