#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/texture.hlsli"
#include "include/shadow.hlsli"

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

float4 main(PixelIn pin) : SV_Target
{
    Texture2D diffuseTex = ResourceDescriptorHeap[gDiffuseTextureIdx];
    Texture2D normalTex = ResourceDescriptorHeap[gNormalTextureIdx];
    Texture2D metallicRoughnessTex = ResourceDescriptorHeap[gMetallicRoughnessTextureIdx];
#ifdef SSAO
    Texture2D ssaoMap = ResourceDescriptorHeap[gAmbientMapRIdx];
#endif
    
    // Interpolation may unnormalize the normal, so renormalize it
    float3 diffuse = diffuseTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb;
    float3 normal = NormalToWorldSpace(normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb, normalize(pin.NormalW), pin.TangentW).rgb;
    float2 metallicRoughness = metallicRoughnessTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).bg;
    
    Material lightMat;
    lightMat.SubsurfaceAlbedo = diffuse;
    lightMat.Metallic = metallicRoughness.r;
    lightMat.Roughness = metallicRoughness.g;

    float3 ambientColor = gAmbientColor * diffuse;
#ifdef SSAO
    float2 ssaoPos = pin.PosH.xy * gInvRenderTargetSize;
    float ambientAccess = ssaoMap.SampleLevel(gsamLinearClamp, ssaoPos, 0.0f).r;
    ambientColor *= ambientAccess;
#endif

    float3 litColor = float3(0.f, 0.f, 0.f);
    float3 toEye = normalize(gEyePosW - pin.PosW);

    uint mainLightIdx;
    float shadowFactor = GetCSMShadowFactor(pin.PosW, pin.PosH, mainLightIdx);
    litColor += shadowFactor * ComputeDirectionalLight(gMainLights[mainLightIdx], lightMat, normal, toEye);

    for (int pointLightIdx = 0; pointLightIdx < gNumPointLights; ++pointLightIdx)
    {
        litColor += ComputePointLight(gPointLights[pointLightIdx], lightMat, pin.PosW, normal, toEye);
    }

    for (int spotLightIdx = 0; spotLightIdx < gNumPointLights; ++spotLightIdx)
    {
        litColor += ComputePointLight(gPointLights[spotLightIdx], lightMat, pin.PosW, normal, toEye);
    }

    return float4(ambientColor + litColor, 1.0f);
}