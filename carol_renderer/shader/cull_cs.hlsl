#include "include/cull.hlsli"
#include "include/mesh.hlsli"

void InitCullMark(uint dtid)
{
    if(dtid % 8 == 0)
    {
        ResetByte(dtid / 8, gInstanceCulledMarkBufferIdx);
#ifdef FRUSTUM
        ResetByte(dtid / 8, gInstanceFrustumCulledMarkBufferIdx);
#endif
#ifdef HIZ_OCCLUSION
        SetByte(dtid / 8, gInstanceOcclusionCulledMarkBufferIdx);
#endif
        DeviceMemoryBarrier();
    }
}

bool InstanceFrustumCull(uint dtid, MeshConstants mc)
{
    float4x4 frustumWorldViewProj = mul(mc.World, gCullViewProj);

    if(dtid % 8 == 0)
    {
        SetByte(dtid / 8, gInstanceFrustumCulledMarkBufferIdx);
    }

    DeviceMemoryBarrier();

    if (AabbFrustumTest(mc.Center, mc.Extents, frustumWorldViewProj) == OUTSIDE)
    {
        SetMark(dtid, gInstanceFrustumCulledMarkBufferIdx);
        return true;
    }

    return false;
}

bool InstanceHiZOcclusionCull(uint dtid, MeshConstants mc)
{
    float4x4 occlusionWorldViewProj = mul(mc.HistWorld, gCullHistViewProj);

    if(dtid % 8 == 0)
    {
        SetByte(dtid / 8, gInstanceOcclusionCulledMarkBufferIdx);
    }

    DeviceMemoryBarrier();

    if(HiZOcclusionTest(mc.Center, mc.Extents, occlusionWorldViewProj, gCullHiZMapIdx))
    {
        ResetMark(dtid, gInstanceOcclusionCulledMarkBufferIdx);
        return false;
    }

    return true;
}

[numthreads(32, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    if(dtid >= gCullMeshCount)
    {
        return;
    }
    
    bool culled = false;
    StructuredBuffer<MeshConstants> meshCB = ResourceDescriptorHeap[gMeshBufferIdx];
    MeshConstants mc = meshCB.Load(dtid);

    InitCullMark(dtid);

#ifdef FRUSTUM
    if(!culled)
    {
        culled |= InstanceFrustumCull(dtid, mc);
    }
#endif
#ifdef HIZ_OCCLUSION
    if(!culled)
    {
        culled |= InstanceHiZOcclusionCull(dtid, mc);
    }
#endif
    
    if(!culled)
    {
        StructuredBuffer<IndirectCommand> commandBuffer = ResourceDescriptorHeap[gCommandBufferIdx];
        AppendStructuredBuffer<IndirectCommand> cullingPassedCommandBuffer = ResourceDescriptorHeap[gCullPassedCommandBufferIdx];

        ResetMark(dtid, gInstanceCulledMarkBufferIdx);
        cullingPassedCommandBuffer.Append(commandBuffer.Load(dtid));
    }
}