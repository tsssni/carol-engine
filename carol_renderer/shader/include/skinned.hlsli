#ifndef SKINNED_HEADER
#define SKINNED_HEADER

#ifdef SKINNED
#include "root_signature.hlsli"

#ifdef SKINNED
cbuffer SkinnedCB : register(b2)
{
    float4x4 gBoneTransforms[256];
    float4x4 gHistBoneTransforms[256];
}
#endif

VertexIn SkinnedTransform(VertexIn vin)
{
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
    
    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(float4(vin.NormalL, 0.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        tangentL += weights[i] * mul(float4(vin.TangentL, 0.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;    
    }
    
    vin.PosL = posL;
    vin.NormalL = normalL;
    vin.TangentL = tangentL;
    
    return vin;
}
#endif

#endif