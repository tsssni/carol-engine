#include "include/cull.hlsli"
#include "include/mesh.hlsli"

bool InstanceFrustumCull(uint dtid, MeshConstants mc)
{
    float4x4 frustumWorldViewProj = mul(mc.World, gCullViewProj);

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