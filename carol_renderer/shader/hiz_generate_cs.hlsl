cbuffer HiZConstants : register(b3)
{
    uint gDepthIdx;
    uint gHiZRIdx;
    uint gHiZWIdx;
    uint gSrcMip;
    uint gNumMipLevel;
    uint gWidth;
    uint gHeight;
}

groupshared float sharedDepth[32][32];

void Init(uint2 dtid)
{
    if(gSrcMip == 0 && dtid.x < gWidth && dtid.y < gHeight)
    {
        RWTexture2D<float4> srcHiZMap = ResourceDescriptorHeap[gHiZWIdx];
        Texture2D depthMap = ResourceDescriptorHeap[gDepthIdx];
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
    uint srcWidth = gWidth >> gSrcMip;
    uint srcHeight = gHeight >> gSrcMip;
    
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
            RWTexture2D<float4> hiZMap = ResourceDescriptorHeap[gHiZWIdx + gSrcMip + i];
            hiZMap[max(dtid >> i, uint2(1, 1))].r = GetMaxDepth(gtid, exp2(i - 1));
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
}