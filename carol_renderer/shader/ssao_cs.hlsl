#include "include/common.hlsli"

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT 14
#endif

#ifndef BLUR_RADIUS
#define BLUR_RADIUS 5
#endif

#ifndef BLUR_COUNT
#define BLUR_COUNT 3
#endif

bool BorderTest(int2 pos, uint2 size)
{
    return pos.x >= 0 && pos.x < size.x && pos.y >= 0 && pos.y < size.y;
}


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
void main( int2 dtid : SV_DispatchThreadID )
{
    uint2 size;
    RWTexture2D<float4> ambientMap = ResourceDescriptorHeap[gAmbientMapWIdx];
    ambientMap.GetDimensions(size.x, size.y);

    if (BorderTest(dtid, size))
    {
        Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
        Texture2D normalMap = ResourceDescriptorHeap[gNormalMapIdx];
        Texture2D randVecMap = ResourceDescriptorHeap[gRandVecMapIdx];

        float2 texC = (dtid + 0.5f) / size;
        float4 posV = GetViewPos(texC);

        float3 normal = normalize(normalMap.Sample(gsamLinearClamp, texC).xyz);
        float viewDepth = NdcDepthToViewDepth(depthMap.Sample(gsamDepthMap, texC).r);
        float3 viewPos = (viewDepth / posV.z) * posV.xyz;

        float3 randVec = 2.0f * randVecMap.Sample(gsamPointWrap, 4.0f * texC).xyz - 1.0f;
        float occlusionSum = 0.0f;
    
        [unroll]
        for (int sampleCount = 0; sampleCount < SAMPLE_COUNT; ++sampleCount)
        {
            float3 offset = reflect(gOffsetVectors[sampleCount].xyz, randVec);
            float flip = sign(dot(offset, normal));
            float3 offsetPos = viewPos + flip * gOcclusionRadius * offset;
        
            float4 screenOffsetPos = mul(float4(offsetPos, 1.0f), gProjTex);
            screenOffsetPos /= screenOffsetPos.w;
            float offsetDepth = NdcDepthToViewDepth(depthMap.Sample(gsamDepthMap, screenOffsetPos.xy).r);
        
            float3 screenPos = (offsetDepth / offsetPos.z) * offsetPos;
            float distZ = viewPos.z - screenPos.z;
            float normalAngle = max(dot(normalize(screenPos - viewPos), normal), 0.0f);
        
            float occlusion = normalAngle * OcclusionFunction(distZ);
            occlusionSum += occlusion;
        }
    
        occlusionSum /= SAMPLE_COUNT;
        float access = 1.0f - occlusionSum;
        ambientMap[dtid] = saturate(pow(access, 6.0f));

        DeviceMemoryBarrier();
        
        float blurWeights[12] =
        {
            gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
            gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
            gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
        };

        [unroll]
        for (int blurCount = 0; blurCount < BLUR_COUNT; ++blurCount)
        {
            [unroll]
            for (int direction = 0; direction < 2; ++direction)
            {
                float2 texOffset = direction ? float2(0.0f, 1.f / size.y) : float2(1.f / size.x, 0.0f);
                int2 uavOffset = direction ? int2(0, 1) : int2(1, 0);
                
                float ambientAccess = blurWeights[BLUR_RADIUS] * ambientMap[dtid].r;
                float totalWeight = blurWeights[BLUR_RADIUS];
        
                [unroll]
                for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i)
                {
                    if (i == 0)
                    {
                        continue;
                    }
            
                    float2 offsetTexC = texC + i * texOffset;
                    int2 offsetUav = dtid + i * uavOffset;

                    float3 neighborNormal = normalMap.Sample(gsamPointClamp, offsetTexC).xyz;
                    float neighborViewDepth = NdcDepthToViewDepth(depthMap.Sample(gsamDepthMap, offsetTexC).r);
     
                    if (BorderTest(offsetUav, size) && dot(normal, neighborNormal) >= 0.8f && abs(viewDepth - neighborViewDepth) <= 0.2f)
                    {
                        ambientAccess += blurWeights[i + BLUR_RADIUS] * ambientMap[offsetUav].r;
                        totalWeight += blurWeights[i + BLUR_RADIUS];
                    }
                }
                
                ambientMap[dtid] = ambientAccess / totalWeight;
                DeviceMemoryBarrier();
            }
        }
     
    }

    
    
}