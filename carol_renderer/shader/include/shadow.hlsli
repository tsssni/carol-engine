#ifndef SHADOW_HEADER
#define SHADOW_HEADER

#define CSM_BLEND_BORDER 0.2f

#include "common.hlsli"
#include "texture.hlsli"

float CalcShadowFactor(float4 shadowPosH, uint shadowMapIdx)
{
    Texture2D gShadowMap = ResourceDescriptorHeap[shadowMapIdx];
    
    // Complete projection by doing division by w.
    shadowPosH = GetTexCoord(shadowPosH);

    // Depth in NDC space.
    float depth = shadowPosH.z;

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
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}

float GetCSMShadowFactor(float3 posW, float4 posH, out uint lightIdx)
{
    float shadowFactor = 1.f;
    
    float mainLightSplitZ[MAIN_LIGHT_SPLIT_LEVEL + 1] =
    {
        gMainLightSplitZ[0].x,
        gMainLightSplitZ[0].y,
        gMainLightSplitZ[0].z,
        gMainLightSplitZ[0].w,
        gMainLightSplitZ[1].x,
        gMainLightSplitZ[1].y
    };

    uint mainLightShadowMapIdx[MAIN_LIGHT_SPLIT_LEVEL] =
    {
        gMainLightShadowMapIdx[0].x,
        gMainLightShadowMapIdx[0].y,
        gMainLightShadowMapIdx[0].z,
        gMainLightShadowMapIdx[0].w,
        gMainLightShadowMapIdx[1].x
    };

    [unroll]
    for (int i = 0; i < MAIN_LIGHT_SPLIT_LEVEL; ++i)
    {
        if (posH.w >= mainLightSplitZ[i] && posH.w < mainLightSplitZ[i + 1])
        {
            lightIdx = i;
            shadowFactor = CalcShadowFactor(mul(float4(posW, 1.0f), gMainLights[i].ViewProj), mainLightShadowMapIdx[i]);

            if (i < MAIN_LIGHT_SPLIT_LEVEL - 1 && (mainLightSplitZ[i + 1] - posH.w) / (mainLightSplitZ[i + 1] - mainLightSplitZ[i]) < CSM_BLEND_BORDER)
            {
                float4 nextLevelShadowPos = mul(float4(posW, 1.0f), gMainLights[i + 1].ViewProj);
                
                if (!CheckOutOfBounds(GetTexCoord(nextLevelShadowPos).xyz))
                {
                    float nextLevelShadowFactor = CalcShadowFactor(nextLevelShadowPos, mainLightShadowMapIdx[i + 1]);
                    float weight = (mainLightSplitZ[i + 1] - posH.w) / (mainLightSplitZ[i + 1] - mainLightSplitZ[i]) / CSM_BLEND_BORDER;
                    shadowFactor = weight * shadowFactor + (1.f - weight) * nextLevelShadowFactor;
                }
            }
            
            break;
        }
    }

    return shadowFactor;
}

#endif
