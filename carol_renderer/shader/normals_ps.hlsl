#include "include/root_signature.hlsli"

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
};

float4 main(PixelIn pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    float3 normalV = mul(float4(pin.NormalW, 0.0f), gView).xyz;
    return float4(normalV, 0.0f);
}

