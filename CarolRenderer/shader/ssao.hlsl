#include "include/ssao.hlsli"
#include "include/root_signature.hlsli"

static const int gSampleCount = 14;

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.TexC = gTexCoords[vid];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    
    float4 posVH = mul(vout.PosH, gInvProj);
    vout.PosV = posVH.xyz / posVH.w;
    
    return vout;
}

float NdcDepthToViewDepth(float depth)
{
    return gProj[3][2] / (depth - gProj[2][2]);
}

float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > gSurfaceEplison)
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
    }
    
    return occlusion;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 normal = normalize(gTex2D[gSsaoNormalMapIdx].SampleLevel(gsamLinearClamp, pin.TexC, 0.0f).xyz);
    float viewDepth = NdcDepthToViewDepth(gTex2D[gSsaoDepthStencilMapIdx].SampleLevel(gsamLinearClamp, pin.TexC, 0.0f).r);
    float3 viewPos = (viewDepth / pin.PosV.z) * pin.PosV.xyz;
    
    float3 randVec = 2.0f * gTex2D[gSsaoRandVecMapIdx].SampleLevel(gsamPointWrap, 4.0f * pin.TexC, 0.0f).xyz - 1.0f;
    float occlusionSum = 0.0f;
    
    for (int i = 0; i < gSampleCount;++i)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
        float flip = sign(dot(offset, normal));
        float3 offsetPos = viewPos + flip * gOcclusionRadius * offset;
        
        float4 screenOffsetPos = mul(float4(offsetPos, 1.0f), gProjTex);
        screenOffsetPos /= screenOffsetPos.w;
        float offsetDepth = NdcDepthToViewDepth(gTex2D[gSsaoDepthStencilMapIdx].SampleLevel(gsamLinearClamp, screenOffsetPos.xy, 0.0f).r);
        
        float3 screenPos = (offsetDepth / offsetPos.z) * offsetPos;
        float distZ = viewPos.z - screenPos.z;
        float dp = max(dot(normalize(screenPos - viewPos), normal), 0.0f);
        
        float occlusion = dp * OcclusionFunction(distZ);
        occlusionSum += occlusion;
    }

    
    occlusionSum /= gSampleCount;
    float access = 1.0f - occlusionSum;
    
    return saturate(pow(access, 6.0f));
}
