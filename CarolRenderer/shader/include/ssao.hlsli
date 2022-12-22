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

    uint gSsaoDepthStencilMapIdx;
    uint gSsaoNormalMapIdx;
    uint gSsaoRandVecMapIdx;
    uint gSsaoAmbientMap0Idx;
    uint gBlurDirection;
    uint3 gSsaoPad0;
}

#endif
