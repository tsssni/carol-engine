#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/texture.hlsli"

struct PixelIn
{
	float4 PosH : SV_POSITION;
    float4 PosVelo : POSITION0;
    float4 PosHist : POSITION1;
    float3 PosW : POSITION2;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

struct PixelOut
{
    float4 DiffuseRoughness : SV_Target0;
    float4 EmissiveMetallic : SV_Target1;
    float4 Normal : SV_Target2;
    float2 Velocity : SV_Target3;
};

PixelOut main(PixelIn pin)
{
    PixelOut pout;

    // Diffuse
    Texture2D diffuseTex = ResourceDescriptorHeap[gDiffuseTextureIdx];
    float3 diffuse = diffuseTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb;
    pout.DiffuseRoughness.rgb = diffuse;

    // Emissive
    Texture2D emissiveTex = ResourceDescriptorHeap[gEmissiveTextureIdx];
    float3 emissive = emissiveTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb;
    pout.EmissiveMetallic.rgb = emissive;

    // Normal
    Texture2D normalTex = ResourceDescriptorHeap[gNormalTextureIdx];
    float3 normal = NormalToWorldSpace(normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb, normalize(pin.NormalW), normalize(pin.TangentW)).rgb;
    pout.Normal = float4(normal, 1.f);
    
    // Metallic & Roughness
    Texture2D metallicRoughnessTex = ResourceDescriptorHeap[gMetallicRoughnessTextureIdx];
    float2 metallicRoughness = metallicRoughnessTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).bg;
    pout.DiffuseRoughness.a = metallicRoughness.g;
    pout.EmissiveMetallic.a = metallicRoughness.r;

    // Velocity
    pin.PosVelo /= pin.PosVelo.w;
    pin.PosHist /= pin.PosHist.w;
    float2 veloPos = gRenderTargetSize * float2(pin.PosVelo.x, -pin.PosVelo.y) * .5f + .5f;
    float2 histPos = gRenderTargetSize * float2(pin.PosHist.x, -pin.PosHist.y) * .5f + .5f;
    pout.Velocity = histPos - veloPos;

    return pout;
}