#ifndef ROOT_SIGNATURE_HEADER
#define ROOT_SIGNATURE_HEADER

#include "light.hlsli"

cbuffer FrameCB : register(b3)
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
    float gFramePad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float2 gFramePad1;

    float4 gOffsetVectors[14];
    float4 gBlurWeights[3];
    
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEplison;

    Light gLights[MAX_LIGHTS];
    
    uint gMeshCBIdx;
    uint gCommandBufferIdx;
    uint gInstanceFrustumCulledMarkIdx;
    uint gInstanceOcclusionPassedMarkIdx;

    uint gFrameMapIdx;
    uint gDepthStencilMapIdx;
    uint gNormalMapIdx;
    uint gMainLightShadowMapIdx;
    uint gOitBufferWIdx;
    uint gOitOffsetBufferWIdx;
    uint gOitCounterBufferIdx;
    uint gOitBufferRIdx;
    uint gOitOffsetBufferRIdx;
    uint gRandVecMapIdx;
    uint gAmbientMapWIdx;
    uint gAmbientMapRIdx;
    uint gVelocityMapIdx;
    uint gHistFrameMapIdx;
    float2 gFramePad2;
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
