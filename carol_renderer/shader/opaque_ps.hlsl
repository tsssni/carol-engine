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
    Texture2D roughnessTex = ResourceDescriptorHeap[gRoughnessTextureIdx];
    Texture2D metallicTex = ResourceDescriptorHeap[gMetallicTextureIdx];
#ifdef SSAO
    Texture2D ssaoMap = ResourceDescriptorHeap[gAmbientMapRIdx];
#endif
    
    float4 texDiffuse = diffuseTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z));
    float metallic = metallicTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).r;
    float roughness = roughnessTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).r;
    
    Material lightMat;
    lightMat.fresnelR0 = CalcFresnelR0(texDiffuse.rgb, metallic);
    lightMat.diffuseAlbedo = texDiffuse.rgb;
    lightMat.roughness = roughness;

    float3 texNormal = normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb;
    texNormal = TexNormalToWorldSpace(texNormal, pin.NormalW, pin.TangentW);
    
    float3 ambient = gLights[0].Ambient;
    
#ifdef SSAO
    float4 ssaoPos = GetTexCoord(pin.PosH);
    float ambientAccess = ssaoMap.SampleLevel(gsamLinearClamp, ssaoPos.xy, 0.0f).r;
    ambient *= ambientAccess;
#endif

    uint lightIdx;
    float shadowFactor = GetCSMShadowFactor(pin.PosW, pin.PosH, lightIdx);
    float4  litColor = float4(shadowFactor * ComputeDirectionalLight(gLights[lightIdx], lightMat, texNormal, normalize(gEyePosW - pin.PosW)), 1.f);
    
    return float4(ambient + litColor.rgb, 1.0f);
}