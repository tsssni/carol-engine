#include "include/common.hlsli"

static const int gSampleCount = 14;

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

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

float4 main(PixelIn pin) : SV_Target
{
    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalIdx];
    Texture2D randVecMap = ResourceDescriptorHeap[gRandVecIdx];
    
    float3 normal = normalize(normalMap.SampleLevel(gsamLinearClamp, pin.TexC, 0.0f).xyz);
    float viewDepth = NdcDepthToViewDepth(depthMap.SampleLevel(gsamLinearClamp, pin.TexC, 0.0f).r);
    float3 viewPos = (viewDepth / pin.PosV.z) * pin.PosV.xyz;
    
    float3 randVec = 2.0f * randVecMap.SampleLevel(gsamPointWrap, 4.0f * pin.TexC, 0.0f).xyz - 1.0f;
    float occlusionSum = 0.0f;
    
    for (int i = 0; i < gSampleCount;++i)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
        float flip = sign(dot(offset, normal));
        float3 offsetPos = viewPos + flip * gOcclusionRadius * offset;
        
        float4 screenOffsetPos = mul(float4(offsetPos, 1.0f), gProjTex);
        screenOffsetPos /= screenOffsetPos.w;
        float offsetDepth = NdcDepthToViewDepth(depthMap.SampleLevel(gsamLinearClamp, screenOffsetPos.xy, 0.0f).r);
        
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
