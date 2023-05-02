#include "include/compute.hlsli"
#include "include/common.hlsli"
#include "include/texture.hlsli"

#ifndef BLUR_COUNT
#define BLUR_COUNT 3
#endif

cbuffer EpfCB : register(b4)
{
    uint ColorMapIdx;
}

groupshared float4 color[32][32];
groupshared float viewDepth[32][32];
groupshared float3 normal[32][32];


float NdcDepthToViewDepth(float depth)
{
    return gProj._43 / (depth - gProj._33);
}

[numthreads(32, 32, 1)]
void main(uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID )
{
    int2 size;
    RWTexture2D<float4> colorMap = ResourceDescriptorHeap[gRWAmbientMapIdx];
    colorMap.GetDimensions(size.x, size.y);

    int2 uid = GetUavId(gid, gtid);
    float2 texC = (uid + 0.5f) / size;

    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
    Texture2D normalMap = ResourceDescriptorHeap[gNormalDepthMapIdx];

    float centerViewDepth = NdcDepthToViewDepth(depthMap.Sample(gsamLinearClamp, texC).r);;
    float3 centerNormal = normalize(normalMap.Sample(gsamLinearClamp, texC).xyz);

    color[gtid.x][gtid.y] = TextureBorderTest(uid, size) ? colorMap[uid] : 0.f;
    viewDepth[gtid.x][gtid.y] = centerViewDepth;
    normal[gtid.x][gtid.y] = centerNormal;

    float blurWeights[12] =
    {
        gGaussWeights[0].x, gGaussWeights[0].y, gGaussWeights[0].z, gGaussWeights[0].w,
        gGaussWeights[1].x, gGaussWeights[1].y, gGaussWeights[1].z, gGaussWeights[1].w,
        gGaussWeights[2].x, gGaussWeights[2].y, gGaussWeights[2].z, gGaussWeights[2].w,
    };

    [unroll]
    for (int blurCount = 0; blurCount < BLUR_COUNT; ++blurCount)
    {
        [unroll]
        for (int direction = 0; direction < 2; ++direction)
        {
            if (!GroupBorderTest(gtid) && TextureBorderTest(uid, size))
            {
                color[gtid.x][gtid.y] = colorMap[uid].r;
            }
            
            GroupMemoryBarrierWithGroupSync();

            if (GroupBorderTest(gtid) && TextureBorderTest(uid, size))
            {
                float2 texOffset = direction ? float2(0.0f, 1.f / size.y) : float2(1.f / size.x, 0.0f);
                int2 uavOffset = direction ? int2(0, 1) : int2(1, 0);
            
                float4 blurredColor = blurWeights[BORDER_RADIUS] * color[gtid.x][gtid.y];
                float totalWeight = blurWeights[BORDER_RADIUS];
    
                [unroll]
                for (int i = -BORDER_RADIUS; i <= BORDER_RADIUS; ++i)
                {
                    if (i == 0)
                    {
                        continue;
                    }
        
                    int2 offsetGtid = gtid + i * uavOffset;
                    int2 offsetUid = uid + i * uavOffset;

                    float3 neighborNormal = normal[offsetGtid.x][offsetGtid.y];
                    float neighborViewDepth = viewDepth[offsetGtid.x][offsetGtid.y];
 
                    if (TextureBorderTest(offsetUid, size) && dot(centerNormal, neighborNormal) >= 0.8f && abs(centerViewDepth - neighborViewDepth) <= 0.2f)
                    {
                        blurredColor += blurWeights[i + BORDER_RADIUS] * color[offsetGtid.x][offsetGtid.y];
                        totalWeight += blurWeights[i + BORDER_RADIUS];
                    }
                }
                
                GroupMemoryBarrierWithGroupSync();
                
                blurredColor /= totalWeight;
                color[gtid.x][gtid.y] = blurredColor;
                colorMap[uid] = blurredColor;

                DeviceMemoryBarrier();
            }
        }
    }
}