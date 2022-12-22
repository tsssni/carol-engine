#include "include/root_signature.hlsli"

#ifdef SKINNED
#include "include/skinned.hlsli"
#endif

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
};

VertexOut VS(VertexIn vin)
{   
    VertexOut vout;
    
#ifdef SKINNED
    vin = SkinnedTransform(vin);
#endif

    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    float3 normalV = mul(float4(pin.NormalW, 0.0f), gView).xyz;
    return float4(normalV, 0.0f);
}
