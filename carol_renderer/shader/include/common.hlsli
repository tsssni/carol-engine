#ifndef ROOT_SIGNATURE_HEADER
#define ROOT_SIGNATURE_HEADER

#define MAX_MAIN_LIGHT_SPLIT_LEVEL 8
#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 64

#include "light.hlsli"

cbuffer FrameCB : register(b3)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;

    float4x4 gVeloViewProj;
    float4x4 gHistViewProj;
    
    float3 gEyePosW;
    float gFramePad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float2 gFramePad1;

    float4 gOffsetVectors[14];
    float4 gGaussWeights[3];
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEplison;

    uint gNumMainLights;
    float3 gAmbientColor;
    Light gMainLights[MAX_MAIN_LIGHT_SPLIT_LEVEL];
    float4 gMainLightSplitZ[2];
    uint4 gMainLightShadowMapIdx[2];

    uint gNumPointLights;
    float3 gFramePad3;
    Light gPointLights[MAX_POINT_LIGHTS];
    
    uint gNumSpotLights;
    float3 gFramePad4;
    Light gSpotLights[MAX_SPOT_LIGHTS];

    uint gFrameHiZMapIdx;
    
    uint gRWFrameMapIdx;
    uint gRWHistMapIdx;
    uint gDepthStencilMapIdx;
    uint gSkyBoxIdx;

    uint gDiffuseRoughnessMapIdx;
    uint gEmissiveMetallicMapIdx;
    uint gNormalMapIdx;
    uint gVelocityMapIdx;
    
    uint gRWOitppllBufferIdx;
    uint gRWOitppllStartOffsetBufferIdx;
    uint gRWOitppllCounterBufferIdx;
    uint gOitppllBufferIdx;
    uint gOitppllStartOffsetBufferIdx;
    
    uint gRWAmbientMapIdx;
    uint gAmbientMapIdx;

    uint gRWSceneColorMapIdx;
    uint gSceneColorMapIdx;
    uint gRWSsgiMapIdx;
    uint gSsgiMapIdx;
    
    uint gRandVecMapIdx;

    float2 gFramePad5;
}

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerState gsamDepthMap : register(s6);
SamplerComparisonState gsamShadow : register(s7);

#endif
