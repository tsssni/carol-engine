#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/texture.hlsli"

struct PixelIn
{
	float4 PosH : SV_POSITION;
    float4 PosHist : POSITION0;
    float3 PosW : POSITION1;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

struct PixelOut
{
    float4 DiffuseRoughness : SV_Target0;
    float4 EmissiveMetallic : SV_Target1;
    float4 NormalDepth : SV_Target2;
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
    float3 normal = NormalToWorldSpace(normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb, normalize(pin.NormalW), pin.TangentW).rgb;
    pout.NormalDepth.rgb = normal;
    
    // Metallic & Roughness
    Texture2D metallicRoughnessTex = ResourceDescriptorHeap[gMetallicRoughnessTextureIdx];
    float2 metallicRoughness = metallicRoughnessTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).bg;
    pout.DiffuseRoughness.a = metallicRoughness.g;
    pout.EmissiveMetallic.a = metallicRoughness.r;

    // Depth
    pout.NormalDepth.a = pin.PosH.w;
    
    // Velocity
    pin.PosHist /= pin.PosHist.w;
    
    float2 histPos;
    histPos.x = (pin.PosHist.x + 1.0f) / 2.0f;
    histPos.y = (1.0f - pin.PosHist.y) / 2.0f;
    histPos *= gRenderTargetSize;
    
    pout.Velocity = histPos - pin.PosH.xy;

    return pout;
}