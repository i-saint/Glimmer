#pragma once
#include "MeshUtils/MeshUtils.h"
using mu::float2;
using mu::float3;
using mu::float4;
using mu::float4x4;
using mu::quatf;
using mu::unorm8x4;
using mu::half4;


// T: unorm8x4, half4, float4
template<class T>
void GenerateCheckerImage(T* pixels, int width, int height);


void GenerateIcoSphereMesh(
    std::vector<int>& counts,
    std::vector<int>& indices,
    std::vector<float3>& points,
    float radius,
    int iteration);

void GenerateWaveMesh(
    std::vector<int>& counts,
    std::vector<int>& indices,
    std::vector<float3> &points,
    float size, float height,
    const int resolution,
    float angle,
    bool triangulate = false);

