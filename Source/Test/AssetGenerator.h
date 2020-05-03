#pragma once
#include "MeshUtils/MeshUtils.h"
using mu::float2;
using mu::float3;
using mu::float4;
using mu::float4x4;
using mu::quatf;
using mu::unorm8x4;
using mu::half4;
using mu::int2;


// T: unorm8x4, half4, float4
template<class T>
void GenerateCheckerImage(T* pixels, int width, int height);

void GenerateCubeMesh(
    RawVector<int>& indices,
    RawVector<float3>& points,
    RawVector<float3>& normals,
    RawVector<float2>& uv,
    float size);


void GenerateIcoSphereMesh(
    RawVector<int>& indices,
    RawVector<float3>& points,
    RawVector<float3>& normals,
    float radius,
    int iteration);

void GenerateWaveMesh(
    RawVector<int>& indices,
    RawVector<float3> &points,
    float size, float height,
    const int resolution,
    float angle);

