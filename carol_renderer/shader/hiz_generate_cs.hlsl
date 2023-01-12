cbuffer HiZConstants : register(b2)
{
    uint gDepthIdx;
    uint gHiZRIdx;
    uint gHiZWIdx;
    uint gSrcMip;
    uint gNumMipLevel;
}

groupshared float sharedDepth[32][32];

void Init(uint2 dtid, uint2 size)
{
    if(gSrcMip == 0 && dtid.x < size.x && dtid.y < size.y)
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
    Texture2D hiZMap = ResourceDescriptorHeap[gHiZRIdx];
    uint2 size;
    uint mip;
    hiZMap.GetDimensions(0, size.x, size.y, mip);
    
    Init(dtid, size);
    uint srcWidth = size.x >> gSrcMip;
    uint srcHeight = size.y >> gSrcMip;
    
    if(dtid.x < srcWidth && dtid.y < srcHeight)
    {
        sharedDepth[gtid.x][gtid.y] = hiZMap.Load(int3(dtid, gSrcMip)).r;
    }
    else
    {
        sharedDepth[gtid.x][gtid.y] = 0.f;
    }
    
    GroupMemoryBarrierWithGroupSync();
        
    for (int i = 1; i <= gNumMipLevel; ++i)
    {
        if (gtid.x % uint(exp2(i)) == 0 && gtid.y % uint(exp2(i)) == 0)
        {
            RWTexture2D<float4> hiZMap = ResourceDescriptorHeap[gHiZWIdx + gSrcMip + i];
            hiZMap[dtid >> i].r = GetMaxDepth(gtid, exp2(i - 1));
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
}