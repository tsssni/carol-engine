#ifndef NORMAL_MAP_HEADER
#define NORMAL_MAP_HEADER
#include "root_signature.hlsli"

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gTex2D[gShadowMapIdx].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    
    for(int i = 0; i < 9; ++i)
    {
        percentLit += gTex2D[gShadowMapIdx].SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}

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