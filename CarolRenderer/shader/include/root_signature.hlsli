#ifndef ROOT_SIGNATURE_HEADER
#define ROOT_SIGNATURE_HEADER

#define MAX_LIGHTS 16

#include "light.hlsli"

cbuffer FrameCB : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gProjTex;
    float4x4 gViewProjTex;

    float4x4 gHistViewProj;
    float4x4 gJitteredViewProj;
    
    float3 gEyePosW;
    float gPassPad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gShadowMapIdx;
    float gSsaoMapIdx;

    Light gLights[MAX_LIGHTS];
}

cbuffer MeshCB : register(b1)
{
    float4x4 gWorld;
    float4x4 gHistWorld;
    
	float3 gFresnelR0;
	float gRoughness;

    uint gMeshDiffuseMapIdx;
    uint gMeshNormalMapIdx;
    uint2 gMeshPad0;
}

#ifdef SKINNED
cbuffer SkinnedCB : register(b2)
{
    float4x4 gBoneTransforms[256];
    float4x4 gHistBoneTransforms[256];
}
#endif

Texture1D gTex1D[] : register(t0, space0);
Texture2D gTex2D[] : register(t0, space1);
Texture3D gTex3D[] : register(t0, space2);
TextureCube gTexCube[] : register(t0, space3);

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

#endif
