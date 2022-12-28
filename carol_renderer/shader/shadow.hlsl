#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

cbuffer LightIdx : register(b4)
{
    uint gLightIdx;
}

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

