#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

#ifdef SKINNED
float3 HistPosL(float3 posL, VertexIn vin)
{
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
    
    float3 histPosL = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        histPosL += weights[i] * mul(float4(posL, 1.0f), gHistBoneTransforms[vin.BoneIndices[i]]).xyz;
    }
    
    return histPosL;
}
#endif

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 PosHist : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float3 posL = vin.PosL;
    
#ifdef SKINNED
    vin = SkinnedTransform(vin);
#endif

    float3 posW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.PosH = mul(float4(posW, 1.0f), gViewProj);

#ifdef SKINNED
    vin.PosL = HistPosL(posL, vin);
#endif
    
    float3 posWHist = mul(float4(vin.PosL, 1.0f), gHistWorld).xyz;
    vout.PosHist = mul(float4(posWHist, 1.0f), gHistViewProj);
    
    return vout;
}

float2 PS(VertexOut pin) : SV_Target
{
    pin.PosHist /= pin.PosHist.w;
    
    float2 histPos;
    histPos.x = (pin.PosHist.x + 1.0f) / 2.0f;
    histPos.y = (1.0f - pin.PosHist.y) / 2.0f;
    histPos *= gRenderTargetSize;
    
    return (histPos - pin.PosH.xy) * gInvRenderTargetSize;
}