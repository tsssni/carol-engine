#ifndef COMMON
#define COMMON

#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#define MAX_LIGHTS 16

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position; 
    float SpotPower;
    float3 Ambient;
    float LightPad0;
    
    float4x4 LightViewProjTex;
};

cbuffer PassCB : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;

    float4x4 gViewProjTex;

    float4x4 gHistViewProj;
    float4x4 gJitteredViewProj;
    
    float3 gEyePosW;
    float gPassPad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gPassPad1;
    float gPassPad2;

    Light gLights[MAX_LIGHTS];
}

cbuffer MeshCB : register(b1)
{
    float4x4 gWorld;
    float4x4 gHistWorld;
    
	float3 gFresnelR0;
	float gRoughness;
}

cbuffer LightCB : register(b2)
{
    float4x4 gLightViewProj;
}

#ifdef SKINNED
cbuffer SkinnedCB : register(b3)
{
    float4x4 gBoneTransforms[256];
    float4x4 gHistBoneTransforms[256];
}
#endif

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC : TEXCOORD;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
#endif
};

float3 TexNormalToWorldSpace(float3 texNormal, float3 pixelNormalW, float3 pixelTangentW)
{
    texNormal = 2.0f * texNormal - 1.0f;
    
    float3 N = pixelNormalW;
    float3 T = pixelTangentW - dot(pixelNormalW, pixelTangentW) * N;
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    return mul(texNormal, TBN);
}

#ifdef SKINNED
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