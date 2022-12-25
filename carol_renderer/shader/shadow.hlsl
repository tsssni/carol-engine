#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

#ifdef SKINNED
#include "include/skinned.hlsli"
#endif

cbuffer ShadowConstant : register(b4)
{
    uint gLightIdx;
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
    vout.PosH = mul(posW, gLights[0].ViewProj);
    
    return vout;
}

