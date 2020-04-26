#pragma once

#include "MeshUtils/MeshUtils.h"
using mu::float2;
using mu::float3;
using mu::float4;

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

