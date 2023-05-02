#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/texture.hlsli"
#include "include/oitppll.hlsli"

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float4 PosHist : POSITION0;
    float3 PosW : POSITION1;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD; 
};

void main(PixelIn pin)
{
    RWStructuredBuffer<OitNode> oitNodeBuffer = ResourceDescriptorHeap[gRWOitppllBufferIdx];
    RWByteAddressBuffer startOffsetBuffer = ResourceDescriptorHeap[gRWOitppllStartOffsetBufferIdx];
    RWByteAddressBuffer counter = ResourceDescriptorHeap[gRWOitppllCounterBufferIdx];
    
    Texture2D diffuseTex = ResourceDescriptorHeap[gDiffuseTextureIdx];
    Texture2D normalTex = ResourceDescriptorHeap[gNormalTextureIdx];
    Texture2D emissiveTex = ResourceDescriptorHeap[gEmissiveTextureIdx];
    Texture2D metallicRoughnessTex = ResourceDescriptorHeap[gMetallicRoughnessTextureIdx];
#ifdef SSAO
    Texture2D ssaoMap = ResourceDescriptorHeap[gAmbientMapIdx];
#endif
    
    // Interpolation may unnormalize the normal, so renormalize it
    float2 uv = pin.PosH.xy * gInvRenderTargetSize;
    float4 diffuse = diffuseTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z));
    float3 normal = NormalToWorldSpace(normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb, normalize(pin.NormalW), pin.TangentW).rgb;
    float3 emissive = emissiveTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).rgb;
    float2 metallicRoughness = metallicRoughnessTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, LOD(pin.PosH.z)).bg;
       
    Material lightMat;
    lightMat.SubsurfaceAlbedo = diffuse.rgb;
    lightMat.Metallic = metallicRoughness.r;
    lightMat.Roughness = max(1e-6f, metallicRoughness.g);

    float3 ambientColor = gAmbientColor * diffuse.rgb;
#ifdef SSAO
    float ambientAccess = ssaoMap.SampleLevel(gsamLinearClamp, uv, 0.0f).r;
    ambientColor *= ambientAccess;
#endif

    float3 toEye = normalize(gEyePosW - pin.PosW);
    float3 emissiveColor = emissive * dot(toEye, normal);
    float3 litColor = float3(0.f, 0.f, 0.f);

    litColor += ComputeDirectionalLight(gMainLights[0], lightMat, normal, toEye);

    for (int pointLightIdx = 0; pointLightIdx < gNumPointLights; ++pointLightIdx)
    {
        litColor += ComputePointLight(gPointLights[pointLightIdx], lightMat, pin.PosW, normal, toEye);
    }

    for (int spotLightIdx = 0; spotLightIdx < gNumPointLights; ++spotLightIdx)
    {
        litColor += ComputePointLight(gPointLights[spotLightIdx], lightMat, pin.PosW, normal, toEye);
    }

    OitNode link;
    link.ColorU = float4(ambientColor + emissiveColor + litColor, diffuse.a);
    link.DepthU = pin.PosH.z * 0xffffffff;

    uint pixelCount;
    counter.InterlockedAdd(0, 1, pixelCount);

    uint2 pixelPos = uint2(pin.PosH.x - 0.5f, pin.PosH.y - 0.5f);

    uint pixelCountAddr = 4 * (pixelPos.y * gRenderTargetSize.x + pixelPos.x);
    uint oldStartOffset;
    startOffsetBuffer.InterlockedExchange(pixelCountAddr, pixelCount, oldStartOffset);

    link.NextU = oldStartOffset;
    oitNodeBuffer[pixelCount] = link;
}