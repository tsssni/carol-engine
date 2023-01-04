#ifndef ROOT_SIGNATURE_HEADER
#define ROOT_SIGNATURE_HEADER

#include "light.hlsli"

cbuffer FrameCB : register(b4)
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
}

cbuffer FrameConstants : register(b5)
{
    uint gFrameIdx;
    uint gDepthStencilIdx;
    uint gHiZRIdx;
    uint gHiZWIdx;
    uint gNormalIdx;
    uint gShadowIdx;
    uint gOitW;
    uint gOitOffsetW;
    uint gOitCounterW;
    uint gOitR;
    uint gOitOffsetR;
    uint gRandVecIdx;
    uint gAmbientIdx;
    uint gVelocityIdx;
    uint gHistIdx;
}

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

#endif
