#include "include/common.hlsli"
#include "include/texture.hlsli"

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT 14
#endif

#ifndef BLUR_RADIUS
#define BLUR_RADIUS 5
#endif

#ifndef BLUR_COUNT
#define BLUR_COUNT 3
#endif

bool UavBorderTest(int2 pos, uint2 size)
{
    return pos.x >= 0 && pos.x < size.x && pos.y >= 0 && pos.y < size.y;
}

bool GroupBorderTest(int2 pos)
{
    return pos.x >= BLUR_RADIUS && pos.x <= 31 - BLUR_RADIUS && pos.y >= BLUR_RADIUS && pos.y <= 31 - BLUR_RADIUS;
}

int2 GetUavId(int2 gid, int2 gtid)
{
    return gid * (32 - 2 * BLUR_RADIUS) + gtid - BLUR_RADIUS;
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

groupshared float ambient[32][32];
groupshared float viewDepth[32][32];
groupshared float3 normal[32][32];

[numthreads(32, 32, 1)]
void main(int2 gid : SV_GroupID, int2 gtid : SV_GroupThreadID)
{
    int2 size;
    RWTexture2D<float4> ambientMap = ResourceDescriptorHeap[gAmbientMapWIdx];
    ambientMap.GetDimensions(size.x, size.y);
    
    int2 uid = GetUavId(gid, gtid);
    float2 texC = (uid + 0.5f) / size;

    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalMapIdx];

    float centerViewDepth = NdcDepthToViewDepth(depthMap.Sample(gsamLinearClamp, texC).r);;
    float3 centerNormal = normalize(normalMap.Sample(gsamLinearClamp, texC).xyz);

    viewDepth[gtid.x][gtid.y] = centerViewDepth;
    normal[gtid.x][gtid.y] = centerNormal;
    
    if (GroupBorderTest(gtid) && UavBorderTest(uid, size))
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
        ambient[gtid.x][gtid.y] = access;
        ambientMap[uid] = access;
    }

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
            if (!GroupBorderTest(gtid) && UavBorderTest(uid, size))
            {
                ambient[gtid.x][gtid.y] = ambientMap[uid].r;
            }
            
            GroupMemoryBarrierWithGroupSync();

            if (GroupBorderTest(gtid) && UavBorderTest(uid, size))
            {
                float2 texOffset = direction ? float2(0.0f, 1.f / size.y) : float2(1.f / size.x, 0.0f);
                int2 uavOffset = direction ? int2(0, 1) : int2(1, 0);
            
                float ambientAccess = blurWeights[BLUR_RADIUS] * ambientMap[uid].r;
                float totalWeight = blurWeights[BLUR_RADIUS];
    
                [unroll]
                for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i)
                {
                    if (i == 0)
                    {
                        continue;
                    }
        
                    int2 offsetGtid = gtid + i * uavOffset;
                    int2 offsetUid = uid + i * uavOffset;

                    float3 neighborNormal = normal[offsetGtid.x][offsetGtid.y];
                    float neighborViewDepth = viewDepth[offsetGtid.x][offsetGtid.y];
 
                    if (UavBorderTest(offsetUid, size) && dot(centerNormal, neighborNormal) >= 0.8f && abs(centerViewDepth - neighborViewDepth) <= 0.2f)
                    {
                        ambientAccess += blurWeights[i + BLUR_RADIUS] * ambient[offsetGtid.x][offsetGtid.y];
                        totalWeight += blurWeights[i + BLUR_RADIUS];
                    }
                }
                
                GroupMemoryBarrierWithGroupSync();
                
                ambientAccess /= totalWeight;
                ambient[gtid.x][gtid.y] = ambientAccess;
                ambientMap[uid] = ambientAccess;

                DeviceMemoryBarrier();
            }
        }
    }
}