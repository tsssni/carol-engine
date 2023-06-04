#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

bool MeshletFrustumCull(uint dtid, CullData cd)
{
    float4x4 frustumWorldViewProj = mul(gWorld, gCullViewProj);

    if(dtid % 8 == 0)
    {
        ResetByte(dtid / 8, gMeshletFrustumCulledMarkBufferIdx);
    }

    DeviceMemoryBarrier();

    if (AabbFrustumTest(cd.Center, cd.Extents, frustumWorldViewProj) == OUTSIDE)
    {
        SetMark(dtid, gMeshletFrustumCulledMarkBufferIdx);
        return true;
    }

    return false;
}

bool MeshletNormalConeCull(uint dtid, CullData cd)
{
    if(dtid % 8 == 0)
    {
        ResetByte(dtid / 8, gMeshletNormalConeCulledMarkBufferIdx);
    }

    DeviceMemoryBarrier();

    if (!NormalConeTest(cd.Center, cd.NormalCone, cd.ApexOffset, gWorld, gCullEyePos))
    {
        SetMark(dtid, gMeshletNormalConeCulledMarkBufferIdx);
        return true;
    }
       
    return false;
}

bool MeshletHiZOcclusionCull(uint dtid, CullData cd)
{
    float4x4 occlusionWorldViewProj = mul(gHistWorld, gCullHistViewProj);

    if(dtid % 8 == 0)
    {
        SetByte(dtid / 8, gMeshletOcclusionCulledMarkBufferIdx);
    }

    DeviceMemoryBarrier();

    if (HiZOcclusionTest(cd.Center, cd.Extents, occlusionWorldViewProj, gCullHiZMapIdx))
    {
        ResetMark(dtid, gMeshletOcclusionCulledMarkBufferIdx);
        return false;
    }

    return true;
}

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    bool visible = false;

#ifdef WRITE
    if(dtid % 8 == 0)
    {
        ResetByte(dtid / 8, gMeshletCulledMarkBufferIdx);
    }

    DeviceMemoryBarrier();
   
    if(dtid < gMeshletCount)
    {
        bool culled = false;
        StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataBufferIdx];
        CullData cd = cullData[dtid];

    #ifdef FRUSTUM
        if(!culled)
        {
            culled |= MeshletFrustumCull(dtid, cd);
        }
    #endif
    #ifdef NORMAL_CONE
        if(!culled)
        {
            culled |= MeshletNormalConeCull(dtid, cd);
        }
    #endif
    #ifdef HIZ_OCCLUSION
        if(!culled)
        {
            culled |= MeshletHiZOcclusionCull(dtid, cd);
        }
    #endif
        visible = !culled;
    }
#else
    visible = dtid < gMeshletCount && !GetMark(dtid, gMeshletCulledMarkBufferIdx);
#endif
    
#if (defined WRITE) && (defined TRANSPARENT)
    if(visible)
    {
        ResetMark(dtid, gMeshletCulledMarkBufferIdx);
    }

    DispatchMesh(0, 0, 0, sharedPayload);
#else
    if (visible)
    {
#ifdef WRITE
        ResetMark(dtid, gMeshletCulledMarkBufferIdx);
#endif
        uint idx = WavePrefixCountBits(visible);
        sharedPayload.MeshletIndices[idx] = dtid;
    }
    
    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, sharedPayload);
#endif
}