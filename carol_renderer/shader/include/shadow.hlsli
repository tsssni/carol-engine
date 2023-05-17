#ifndef SHADOW_HEADER
#define SHADOW_HEADER

#define CSM_BLEND_BORDER 0.2f

#include "common.hlsli"
#include "texture.hlsli"
#include "transform.hlsli"

float CalcShadowFactor(float2 pos, float depth, uint shadowMapIdx)
{
    Texture2D gShadowMap = ResourceDescriptorHeap[shadowMapIdx];
    
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    [unroll]
    for(int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            pos.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}

float GetCSMShadowFactor(float3 posW, float viewDepth, out uint lightIdx)
{
    float shadowFactor = 1.f;
    
    float mainLightShadowMapIdx[MAIN_LIGHT_SPLIT_LEVEL];
    [unroll]
    for (int shadowIdx = 0; shadowIdx < MAIN_LIGHT_SPLIT_LEVEL;++shadowIdx)
    {
        mainLightShadowMapIdx[shadowIdx] = gMainLightShadowMapIdx[shadowIdx / 4][shadowIdx % 4];
    }
    
    float mainLightSplitZ[MAIN_LIGHT_SPLIT_LEVEL + 1];
    [unroll]
    for (int splitIdx = 0; splitIdx < MAIN_LIGHT_SPLIT_LEVEL + 1;++splitIdx)
    {
        mainLightSplitZ[splitIdx] = gMainLightSplitZ[splitIdx / 4][splitIdx % 4];
    }

    [unroll]
    for (int i = 0; i < MAIN_LIGHT_SPLIT_LEVEL; ++i)
    {
        if (viewDepth >= mainLightSplitZ[i] && viewDepth < mainLightSplitZ[i + 1])
        {
            lightIdx = i;
            float4 currLevelShadowPos = mul(float4(posW, 1.0f), gMainLights[i].ViewProj);
            float4 currLevelShadowNdcPos = ProjPosToNdcPos(currLevelShadowPos);
            float2 currLevelShadowTexPos = NdcTexPosToTexPos(currLevelShadowNdcPos.xy);

            shadowFactor = CalcShadowFactor(currLevelShadowTexPos, currLevelShadowNdcPos.z, mainLightShadowMapIdx[i]);            
            break;
        }
    }

    return shadowFactor;
}

#endif
