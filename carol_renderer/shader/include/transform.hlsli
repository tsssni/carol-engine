#ifndef TRANSFORM_HEADER
#define TRANSFORM_HEADER

float NdcDepthToViewDepth(float depth, float4x4 proj)
{
    return proj._43 / (depth - proj._33);
}

float4 ProjPosToNdcPos(float4 pos)
{
    return pos / pos.w;
}

float2 NdcTexPosToTexPos(float2 pos)
{
    return float2(0.5f * (pos.x + 1.f), 0.5f * (1.f - pos.y));
}

float2 TexPosToNdcTexPos(float2 pos)
{
    return float2(2.f * pos.x - 1.f, 1.f - 2.f * pos.y);
}

float2 ProjPosToTexPos(float4 pos)
{
    return NdcTexPosToTexPos(ProjPosToNdcPos(pos).xy);
}

float3 NdcTexPosToViewPos(float2 pos, float viewDepth, float4x4 invProj)
{
    float4 posV = mul(float4(pos.x, pos.y, 0.f, 1.f), invProj);
    posV /= posV.w;
    posV.xyz *= viewDepth / posV.z;

    return posV.xyz;
}

float3 TexPosToViewPos(float2 pos, float viewDepth, float4x4 invProj)
{
    return NdcTexPosToViewPos(TexPosToNdcTexPos(pos), viewDepth, invProj);
}

float3 ViewPosToWorldPos(float3 pos, float4x4 gInvView)
{
    float4 posW = mul(float4(pos, 1.f), gInvView);
    return posW.xyz / posW.w;
}
#endif
