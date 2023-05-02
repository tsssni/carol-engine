#include "include/common.hlsli"
#include "include/compute.hlsli"
#include "include/texture.hlsli"

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT 14
#endif

#ifndef BLUR_COUNT
#define BLUR_COUNT 3
#endif

float4 GetViewPos(float2 pos)
{
    float4 posV = mul(float4(2.f * pos.x - 1.f, 1.f - 2.f * pos.y, 0.f, 1.f), gInvProj);
    return posV / posV.w;
}

float NdcDepthToViewDepth(float depth)
{
    return gProj._43 / (depth - gProj._33);
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

[numthreads(32, 32, 1)]
void main(int2 dtid : SV_DispatchThreadID)
{
    uint2 size;
    RWTexture2D<float4> ambientMap = ResourceDescriptorHeap[gRWAmbientMapIdx];
    ambientMap.GetDimensions(size.x, size.y);
    
    float2 texC = (dtid + 0.5f) / size;

    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalDepthMapIdx];

    float centerViewDepth = NdcDepthToViewDepth(depthMap.Sample(gsamLinearClamp, texC).r);;
    float3 centerNormal = normalize(normalMap.Sample(gsamLinearClamp, texC).xyz);

    if (TextureBorderTest(dtid, size))
    {
        Texture2D randVecMap = ResourceDescriptorHeap[gRandVecMapIdx];
        float4 posV = GetViewPos(texC);
        float3 viewPos = (centerViewDepth / posV.z) * posV.xyz;

        float3 randVec = 2.0f * randVecMap.Sample(gsamPointWrap, 4.0f * texC).xyz - 1.0f;
        float occlusionSum = 0.0f;
    
        [unroll]
        for (int sampleCount = 0; sampleCount < SAMPLE_COUNT; ++sampleCount)
        {
            float3 offset = reflect(gOffsetVectors[sampleCount].xyz, randVec);
            float flip = sign(dot(offset, centerNormal));
            float3 offsetPos = viewPos + flip * gOcclusionRadius * offset;
        
            float2 screenOffsetPos = GetTexCoord(mul(float4(offsetPos, 1.0f), gProj)).xy;
            float offsetDepth = NdcDepthToViewDepth(depthMap.Sample(gsamDepthMap, screenOffsetPos).r);
        
            float3 screenPos = (offsetDepth / offsetPos.z) * offsetPos;
            float distZ = viewPos.z - screenPos.z;
            float normalAngle = max(dot(normalize(screenPos - viewPos), centerNormal), 0.0f);
        
            float occlusion = normalAngle * OcclusionFunction(distZ);
            occlusionSum += occlusion;
        }
    
        occlusionSum /= SAMPLE_COUNT;
        float access = saturate(pow(1.0f - occlusionSum, 6.f));
        ambientMap[dtid] = access;
    }
}