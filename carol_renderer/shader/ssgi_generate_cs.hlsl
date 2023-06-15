#include "include/common.hlsli"
#include "include/compute.hlsli"
#include "include/texture.hlsli"

cbuffer SceneColorConstants : register(b2)
{
    uint gSrcMip;
    uint gNumMipLevel;
}

void Init(uint2 dtid, uint2 size)
{
    if(gSrcMip == 0 && dtid.x < size.x && dtid.y < size.y)
    {
        RWTexture2D<float4> srcSceneColorMap = ResourceDescriptorHeap[gRWSceneColorMapIdx];
        RWTexture2D<float4> srcSsgiHiZMap = ResourceDescriptorHeap[gRWSsgiHiZMapIdx];

        RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
        Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];

        srcSceneColorMap[dtid] = frameMap[dtid];
        srcSsgiHiZMap[dtid] = depthMap[dtid];
    }
}

groupshared float depth[32][32];
groupshared float3 color[32][32];

float4 GetColorDepth(uint2 gtid, uint offset)
{
    float d[4] =
    {
        depth[gtid.x][gtid.y],
        depth[gtid.x + offset][gtid.y],
        depth[gtid.x][gtid.y + offset],
        depth[gtid.x + offset][gtid.y + offset]
    };
    
    float minD = min(d[0], min(d[1], min(d[2], d[3])));

    float3 c[4] =
    {
        color[gtid.x][gtid.y],
        color[gtid.x + offset][gtid.y],
        color[gtid.x][gtid.y + offset],
        color[gtid.x + offset][gtid.y + offset]
    };
    
    float3 sceneColor = c[0];

    [unroll]
    for (int i = 1; i < 4;++i)
    {
        float weight = 1.f - (d[i] - minD);
        weight *= weight;
        sceneColor += weight * c[i];
    }

    sceneColor /= 4.f;
    float minDepth = min(d[0], min(d[1], min(d[2], d[3])));

    color[gtid.x][gtid.y] = sceneColor;
    depth[gtid.x][gtid.y] = minDepth;
    
    return float4(sceneColor, minDepth);
}

[numthreads(32, 32, 1)]
void main( uint2 dtid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID)
{    
    RWTexture2D<float4> srcSceneColorMap = ResourceDescriptorHeap[gRWSceneColorMapIdx + gSrcMip];
    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];

    uint2 size;
    srcSceneColorMap.GetDimensions(size.x, size.y);
    Init(dtid, size);
    
    if (TextureBorderTest(dtid, size))
    {
        depth[gtid.x][gtid.y] = depthMap[dtid].r;
        color[gtid.x][gtid.y] = srcSceneColorMap[dtid].rgb;
    }
    else
    {
        depth[gtid.x][gtid.y] = 1.f;
        color[gtid.x][gtid.y] = 0.f;
    }
    
    GroupMemoryBarrierWithGroupSync();
        
    for (int i = 1; i <= gNumMipLevel; ++i)
    {
        if (gtid.x % uint(exp2(i)) == 0 && gtid.y % uint(exp2(i)) == 0)
        {
            RWTexture2D<float4> writeSceneColorMap = ResourceDescriptorHeap[gRWSceneColorMapIdx + gSrcMip + i];
            RWTexture2D<float4> writeSsgiHiZMap = ResourceDescriptorHeap[gRWSsgiHiZMapIdx + gSrcMip + i];

            if (TextureBorderTest(dtid, size))
            {
                float4 colorDepth = GetColorDepth(gtid, exp2(i - 1));
                writeSceneColorMap[dtid>>i].rgb = colorDepth.rgb;
                writeSsgiHiZMap[dtid>>i].r = colorDepth.a;

                GroupMemoryBarrierWithGroupSync();
            }
        }
    }
}