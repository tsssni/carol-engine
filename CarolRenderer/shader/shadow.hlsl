#include "include/root_signature.hlsli"

#ifdef SKINNED
#include "include/skinned.hlsli"
#endif

cbuffer ShadowCB : register(b3)
{
    uint gLightIdx;
    uint3 gShadowPad0;
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
#ifdef SKINNED
    vin = SkinnedTransform(vin);
#endif
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gLights[gLightIdx].ViewProj);
    
    return vout;
}

