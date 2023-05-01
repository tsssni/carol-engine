#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

bool MeshletHiZOcclusionCull(uint dtid, CullData cd)
{
    float4x4 occlusionWorldViewProj = mul(gWorld, gCullViewProj);

    if (HiZOcclusionTest(cd.Center, cd.Extents, occlusionWorldViewProj, gCullHiZMapIdx))
    {
        ResetMark(dtid, gMeshletNormalConeCulledMarkBufferIdx);
        return false;
    }

    return true;
}

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    bool visible = false;

#ifdef WRITE
    if (dtid < gMeshletCount 
        && !GetMark(dtid, gMeshletFrustumCulledMarkBufferIdx) 
        && !GetMark(dtid, gMeshletNormalConeCulledMarkBufferIdx) 
        && GetMark(dtid, gMeshletOcclusionCulledMarkBufferIdx))
    {
        StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataBufferIdx];
        CullData cd = cullData[dtid];

        visible = !MeshletHiZOcclusionCull(dtid, cd);
    }
#else
    visible = dtid < gMeshletCount && !GetMark(dtid, gMeshletCulledMarkBufferIdx);
#endif 
    
#if (defined WRITE) && (defined TRANSPARENT)
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