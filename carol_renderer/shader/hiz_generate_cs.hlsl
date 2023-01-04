#include "include/common.hlsli"

cbuffer HiZConstants : register(b3)
{
    uint gSrcMip;
    uint gNumMipLevel;
}

groupshared float sharedDepth[32][32];

void Init(uint2 dtid)
{
    if(gSrcMip == 0 && dtid.x < gRenderTargetSize.x && dtid.y < gRenderTargetSize.y)
    {
        RWTexture2D<float4> srcHiZMap = ResourceDescriptorHeap[gHiZWIdx];
        Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilIdx];
        srcHiZMap[dtid].r = depthMap.Load(int3(dtid, 0)).r;
    }
}

float GetMaxDepth(uint2 gtid, uint offset)
{
    float d0 = sharedDepth[gtid.x][gtid.y];
    float d1 = sharedDepth[gtid.x + offset][gtid.y];
    float d2 = sharedDepth[gtid.x][gtid.y + offset];
    float d3 = sharedDepth[gtid.x + offset][gtid.y + offset];
    
    float maxDepth = max(d0, max(d1, max(d2, d3)));
    sharedDepth[gtid.x][gtid.y] = maxDepth;
    
    return maxDepth;
}

[numthreads(32, 32, 1)]
void main( uint2 dtid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID)
{    
    Init(dtid);
    uint srcWidth = uint(gRenderTargetSize.x) >> gSrcMip;
    uint srcHeight = uint(gRenderTargetSize.y) >> gSrcMip;
    
    if(dtid.x < srcWidth && dtid.y < srcHeight)
    {
        Texture2D hizMap = ResourceDescriptorHeap[gHiZRIdx];
        sharedDepth[gtid.x][gtid.y] = hizMap.Load(int3(dtid, gSrcMip)).r;
    }
    else
    {
        sharedDepth[gtid.x][gtid.y] = 0.f;
    }
    
    GroupMemoryBarrierWithGroupSync();
        
    for (int i = 1; i <= gNumMipLevel; ++i)
    {
        if (gtid.x % (uint) exp2(i) == 0 && gtid.y % (uint) exp2(i) == 0)
        {
            RWTexture2D<float4> hizMap = ResourceDescriptorHeap[gHiZWIdx + gSrcMip + i];
            hizMap[dtid >> i].r = GetMaxDepth(gtid, exp2(i - 1));
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
}