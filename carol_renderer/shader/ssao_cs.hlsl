#include "include/common.hlsli"
#include "include/compute.hlsli"
#include "include/texture.hlsli"
#include "include/transform.hlsli"

cbuffer SsaoCB : register(b4)
{
    uint gSampleCount;
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
    Texture2D normalMap = ResourceDescriptorHeap[gNormalMapIdx];

    float ndcDepth = depthMap.Sample(gsamLinearClamp, texC).r;

    if (ndcDepth < 1.f && TextureBorderTest(dtid, size))
    {
        float centerViewDepth = NdcDepthToViewDepth(ndcDepth, gProj);
        float3 centerNormal = mul(normalize(normalMap.Sample(gsamLinearClamp, texC).xyz), (float3x3) gView);

        Texture2D randVecMap = ResourceDescriptorHeap[gRandVecMapIdx];
        float3 viewPos = TexPosToViewPos(texC, centerViewDepth, gInvProj);

        float3 randVec = 2.0f * randVecMap.Sample(gsamPointWrap, 4.0f * texC).xyz - 1.0f;
        float occlusionSum = 0.0f;
    
        for (int sampleCount = 0; sampleCount < gSampleCount; ++sampleCount)
        {
            float3 offset = reflect(gOffsetVectors[sampleCount].xyz, randVec);
            float flip = sign(dot(offset, centerNormal));
            float3 offsetPos = viewPos + flip * gOcclusionRadius * offset;
        
            float2 offsetTexPos = ProjPosToTexPos(mul(float4(offsetPos, 1.0f), gProj));
            float offsetDepth = NdcDepthToViewDepth(depthMap.Sample(gsamDepthMap, offsetTexPos).r, gProj);
            float3 offsetViewPos = TexPosToViewPos(offsetTexPos, offsetDepth, gInvProj);

            float distZ = viewPos.z - offsetViewPos.z;
            float ndotv = max(dot(normalize(offsetViewPos - viewPos), centerNormal), 0.0f);
        
            float occlusion = ndotv * OcclusionFunction(distZ);
            occlusionSum += occlusion;
        }
    
        occlusionSum /= gSampleCount;
        float access = saturate(pow(1.0f - occlusionSum, 6.f));
        ambientMap[dtid] = access;
    }
}