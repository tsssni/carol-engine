#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/texture.hlsli"
#include "include/shadow.hlsli"

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD; 
#ifdef SSAO
    float4 SsaoPosH : POSITION1;
#endif

};

float4 main(PixelIn pin) : SV_Target
{
    Texture2D diffuseTex = ResourceDescriptorHeap[gDiffuseTextureIdx];
    Texture2D normalTex = ResourceDescriptorHeap[gNormalTextureIdx];

#ifdef SSAO
    Texture2D ssaoMap = ResourceDescriptorHeap[gAmbientMapRIdx];
#endif
    
    float4 texDiffuse = diffuseTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z));
    
    LightMaterialData lightMat;
    lightMat.fresnelR0 = gFresnelR0;
    lightMat.diffuseAlbedo = texDiffuse.rgb;
    lightMat.roughness = gRoughness;

    float3 texNormal = normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb;
    texNormal = TexNormalToWorldSpace(texNormal, pin.NormalW, pin.TangentW);
    
    float3 ambient = 0.4f * texDiffuse.rgb;
    
#ifdef SSAO
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = ssaoMap.SampleLevel(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;
    ambient *= ambientAccess;
#endif

    float shadowFactor = 1.f;
    float4 litColor = float4(1.f, 1.f, 1.f, 1.f);

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
        if (pin.PosH.w >= mainLightSplitZ[i] && pin.PosH.w < mainLightSplitZ[i + 1])
        {
            shadowFactor = CalcShadowFactor(mul(float4(pin.PosW, 1.0f), gLights[i].ViewProj), mainLightShadowMapIdx[i]);

            if (i < MAIN_LIGHT_SPLIT_LEVEL - 1 && (mainLightSplitZ[i + 1] - pin.PosH.w) / (mainLightSplitZ[i + 1] - mainLightSplitZ[i]) < CSM_BLEND_BORDER)
            {
                float4 nextLevelShadowPos = mul(float4(pin.PosW, 1.0f), gLights[i + 1].ViewProj);
                
                if (!CheckOutOfBounds(GetTexCoord(nextLevelShadowPos).xyz))
                {
                    float nextLevelShadowFactor = CalcShadowFactor(nextLevelShadowPos, mainLightShadowMapIdx[i + 1]);
                    float weight = (mainLightSplitZ[i + 1] - pin.PosH.w) / (mainLightSplitZ[i + 1] - mainLightSplitZ[i]) / CSM_BLEND_BORDER;
                    shadowFactor = weight * shadowFactor + (1.f - weight) * nextLevelShadowFactor;
                }
            }
            
            litColor = float4(shadowFactor * ComputeDirectionalLight(gLights[i], lightMat, texNormal, normalize(gEyePosW - pin.PosW)), 1.f);
            break;
        }
    }
    
    return float4(ambient + litColor.rgb, 1.0f);
}