#ifndef SSAO_HEADER
#define SSAO_HEADER

#include "screen_tex.hlsli"

cbuffer SsaoCB : register(b3)
{
    float4 gOffsetVectors[14];
    float4 gBlurWeights[3];
    
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEplison;
}

cbuffer BlurConstant : register(b4)
{
    uint gBlurDirection;
}

Texture2D gDepthMap : register(t0);
Texture2D gNormalMap : register(t1);

#endif
