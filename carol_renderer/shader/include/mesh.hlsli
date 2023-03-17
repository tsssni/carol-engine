#ifndef MESH_HEADER
#define MESH_HEADER

#include "common.hlsli"

cbuffer MeshCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gHistWorld;
    
    float3 gCenter;
    float MeshPad0;
    float3 gExtents;
    float MeshPad1;
    
    uint gMeshletCount;
    uint gVertexBufferIdx;
    uint gMeshletBufferIdx;
    uint gCullDataBufferIdx;
    
    uint gMeshletFrustumCulledMarkBufferIdx;
    uint gMeshletNormalConeCulledMarkBufferIdx;
    uint gMeshletOcclusionCulledMarkBufferIdx;
    uint gMeshletCulledMarkBufferIdx;
    
    uint gDiffuseTextureIdx;
    uint gNormalTextureIdx;
    uint gMetallicRoughnessTextureIdx;
    float MeshPad3;
};

cbuffer SkinnedCB : register(b1)
{
    float4x4 gBoneTransforms[256];
    float4x4 gHistBoneTransforms[256];
};

struct Meshlet
{
    uint Vertices[64];
    uint Prims[126];
    uint VertexCount;
    uint PrimCount;
};

struct MeshIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC : TEXCOORD;
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
};

uint3 UnpackPrim(uint prim)
{
    return uint3(prim & 0x3FF, (prim >> 10) & 0x3FF, (prim >> 20) & 0x3FF);
}

MeshIn SkinnedTransform(MeshIn min)
{
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = min.BoneWeights.x;
    weights[1] = min.BoneWeights.y;
    weights[2] = min.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
    
    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        posL += weights[i] * mul(float4(min.PosL, 1.0f), gBoneTransforms[min.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(float4(min.NormalL, 0.0f), gBoneTransforms[min.BoneIndices[i]]).xyz;
        tangentL += weights[i] * mul(float4(min.TangentL, 0.0f), gBoneTransforms[min.BoneIndices[i]]).xyz;    
    }
    
    min.PosL = posL;
    min.NormalL = normalL;
    min.TangentL = tangentL;
    
    return min;
}

#endif