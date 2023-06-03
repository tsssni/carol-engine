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
        RWTexture2D<float4> histMap = ResourceDescriptorHeap[gRWHistMapIdx];
        srcSceneColorMap[dtid] = histMap[dtid];
    }
}

groupshared float depth[32][32];
groupshared float4 color[32][32];

float4 GetSceneColor(uint2 gtid, uint offset)
{
    float d[4] =
    {
        depth[gtid.x][gtid.y],
        depth[gtid.x + offset][gtid.y],
        depth[gtid.x][gtid.y + offset],
        depth[gtid.x + offset][gtid.y + offset]
    };

    float4 c[4] =
    {
        color[gtid.x][gtid.y],
        color[gtid.x + offset][gtid.y],
        color[gtid.x][gtid.y + offset],
        color[gtid.x + offset][gtid.y + offset]
    };
    
    float4 sceneColor = c[0];

    [unroll]
    for (int i = 1; i < 4;++i)
    {
        float weight = 1.f - abs(d[0] - d[i]);
        weight *= weight;
        sceneColor += weight * c[i];
    }

    sceneColor /= 4.f;
    color[gtid.x][gtid.y] = sceneColor;

    return sceneColor;
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
        color[gtid.x][gtid.y] = srcSceneColorMap[dtid];
    }
    else
    {
        depth[gtid.x][gtid.y] = 0.f;
        color[gtid.x][gtid.y] = 0.f;
    }
    
    GroupMemoryBarrierWithGroupSync();
        
    for (int i = 1; i <= gNumMipLevel; ++i)
    {
        if (gtid.x % uint(exp2(i)) == 0 && gtid.y % uint(exp2(i)) == 0)
        {
            RWTexture2D<float4> writeSceneColorMap = ResourceDescriptorHeap[gRWSceneColorMapIdx + gSrcMip + i];

            if (TextureBorderTest(dtid, size))
            {
                writeSceneColorMap[dtid>>i] = GetSceneColor(gtid, exp2(i - 1));
            }
        }
        
        DeviceMemoryBarrierWithGroupSync();
    }
}