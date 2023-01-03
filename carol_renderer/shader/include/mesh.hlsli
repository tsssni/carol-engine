#ifndef MESH_HEADER
#define MESH_HEADER

cbuffer MeshCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gHistWorld;
    
    float3 gCenter;
    uint gMeshletCount;
    float3 gExtents;
    float gMeshPad0;
    
    float3 gFresnelR0;
    float gRoughness;
}

cbuffer MeshConstants : register(b1)
{
    uint gVertexIdx;
    uint gMeshletIdx;
    uint gCullDataIdx;
    uint gDiffuseMapIdx;
    uint gNormalMapIdx;
}

#ifdef SKINNED
cbuffer SkinnedCB : register(b2)
{
    float4x4 gBoneTransforms[256];
    float4x4 gHistBoneTransforms[256];
}
#endif

struct Meshlet
{
    uint Vertices[64];
    uint Prims[126];
    uint VertexCount;
    uint PrimCount;
};

struct CullData
{
    float3 Center;
    float3 Extents;
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

#ifdef SKINNED
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