#ifndef MESH_HEADER
#define MESH_HEADER
#include "root_signature.hlsli"

cbuffer MeshCB : register(b1)
{
    float4x4 gWorld;
    float4x4 gHistWorld;
    
	float3 gFresnelR0;
	float gRoughness;
    
    uint gDiffuseMapIdx;
    uint gNormalMapIdx;
    uint2 gMeshPad0;
}

float3 TexNormalToWorldSpace(float3 texNormal, float3 pixelNormalW, float3 pixelTangentW)
{
    texNormal = 2.0f * texNormal - 1.0f;
    
    float3 N = pixelNormalW;
    float3 T = pixelTangentW - dot(pixelNormalW, pixelTangentW) * N;
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    return mul(texNormal, TBN);
}

#endif