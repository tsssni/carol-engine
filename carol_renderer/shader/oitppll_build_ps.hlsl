#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/oitppll.hlsli"
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

void main(PixelIn pin)
{
    
    
    
    RWStructuredBuffer<OitNode> oitNodeBuffer = ResourceDescriptorHeap[gOitBufferWIdx];
    RWByteAddressBuffer startOffsetBuffer = ResourceDescriptorHeap[gOitOffsetBufferWIdx];
    RWByteAddressBuffer counter = ResourceDescriptorHeap[gOitCounterBufferIdx];
    
    Texture2D diffuseTex = ResourceDescriptorHeap[gDiffuseTextureIdx];
    Texture2D normalTex = ResourceDescriptorHeap[gNormalTextureIdx];
    Texture2D depthMap = ResourceDescriptorHeap[gNormalMapIdx];
#ifdef SSAO
    Texture2D ssaoMap = ResourceDescriptorHeap[gAmbientMapIdx];
#endif
    
    float2 ndcPos = pin.PosH.xy * gInvRenderTargetSize;
    if (pin.PosH.z > depthMap.Sample(gsamAnisotropicWrap, ndcPos).r)
    {
        return;
    }

    float4 texDiffuse = diffuseTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, pow(pin.PosH.z, 15.0f) * 8.0f);

    LightMaterialData lightMat;
    lightMat.fresnelR0 = gFresnelR0;
    lightMat.diffuseAlbedo = texDiffuse.rgb;
    lightMat.roughness = gRoughness;

    float3 texNormal = normalTex.SampleLevel(gsamAnisotropicWrap, pin.TexC, pow(pin.PosH.z, 15.0f) * 8.0f).rgb;
    texNormal = TexNormalToWorldSpace(texNormal, pin.NormalW, pin.TangentW);

    float3 ambient = gLights[0].Ambient * texDiffuse.rgb;

#ifdef SSAO
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = ssaoMap.SampleLevel(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;
    ambient*=ambientAccess;
#endif

    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(mul(float4(pin.PosW, 1.0f), gLights[0].ViewProjTex), gMainLightShadowMapIdx);
    float4 litColor = ComputeLighting(gLights, lightMat, pin.PosW, texNormal, normalize(gEyePosW - pin.PosW), shadowFactor);
    float3 color = ambient + litColor.rgb;

    OitNode link;
    link.ColorU = float4(color.rgb, 0.6f);
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