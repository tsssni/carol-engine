#include "common.hlsli"

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
    vout.PosH = mul(posW, gLightViewProj);
    
    return vout;
}

