#include "include/compute.hlsli"

cbuffer HiZConstants : register(b2)
{
    uint gDepthIdx;
    uint gHiZIdx;
    uint gRWHiZIdx;
    uint gSrcMip;
    uint gNumMipLevel;
}

void Init(uint2 dtid, uint2 size)
{
    if(gSrcMip == 0 && dtid.x < size.x && dtid.y < size.y)
    {
        RWTexture2D<float4> srcHiZMap = ResourceDescriptorHeap[gRWHiZIdx];
        Texture2D depthMap = ResourceDescriptorHeap[gDepthIdx];
        srcHiZMap[dtid].r = depthMap.Load(int3(dtid, 0)).r;
    }
}

groupshared float depth[32][32];

float GetMaxDepth(uint2 gtid, uint offset)
{
    float d0 = depth[gtid.x][gtid.y];
    float d1 = depth[gtid.x + offset][gtid.y];
    float d2 = depth[gtid.x][gtid.y + offset];
    float d3 = depth[gtid.x + offset][gtid.y + offset];
    
    float maxDepth = max(d0, max(d1, max(d2, d3)));
    depth[gtid.x][gtid.y] = maxDepth;
    
    return maxDepth;
}

[numthreads(32, 32, 1)]
void main( uint2 dtid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID)
{    
    RWTexture2D<float4> srcHiZMap = ResourceDescriptorHeap[gRWHiZIdx + gSrcMip];
    uint2 size;
    srcHiZMap.GetDimensions(size.x, size.y);
    Init(dtid, size);
    
    if (UavBorderTest(dtid, size))
    {
        depth[gtid.x][gtid.y] = srcHiZMap[dtid].r;
    }
    else
    {
        depth[gtid.x][gtid.y] = 0.f;
    }
    
    GroupMemoryBarrierWithGroupSync();
        
    for (int i = 1; i <= gNumMipLevel; ++i)
    {
        if (gtid.x % uint(exp2(i)) == 0 && gtid.y % uint(exp2(i)) == 0)
        {
            RWTexture2D<float4> writeHiZMap = ResourceDescriptorHeap[gRWHiZIdx + gSrcMip + i];

            if (UavBorderTest(dtid, size))
            {
                writeHiZMap[dtid>>i].r = GetMaxDepth(gtid, exp2(i - 1));
            }
        }
        
        DeviceMemoryBarrierWithGroupSync();
    }
}