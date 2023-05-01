#include "include/cull.hlsli"
#include "include/mesh.hlsli"

bool InstanceHiZOcclusionCull(uint dtid, MeshConstants mc)
{
    float4x4 occlusionWorldViewProj = mul(mc.World, gCullViewProj);
        
    if(HiZOcclusionTest(mc.Center, mc.Extents, occlusionWorldViewProj, gCullHiZMapIdx))
    {
        ResetMark(gCullMeshOffset + dtid, gInstanceOcclusionCulledMarkBufferIdx);
        return false;
    }

    return true;
}

[numthreads(32, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    if(dtid >= gCullMeshCount 
        || GetMark(gCullMeshOffset + dtid, gInstanceFrustumCulledMarkBufferIdx)
        || !GetMark(gCullMeshOffset + dtid, gInstanceOcclusionCulledMarkBufferIdx))
    {
        return;
    }
    
    StructuredBuffer<MeshConstants> meshCB = ResourceDescriptorHeap[gMeshBufferIdx];
    MeshConstants mc = meshCB.Load(gCullMeshOffset + dtid);

    if (!InstanceHiZOcclusionCull(dtid, mc))
    {
        StructuredBuffer<IndirectCommand> commandBuffer = ResourceDescriptorHeap[gCommandBufferIdx];
        AppendStructuredBuffer<IndirectCommand> cullingPassedCommandBuffer = ResourceDescriptorHeap[gCullPassedCommandBufferIdx];

        ResetMark(dtid, gInstanceCulledMarkBufferIdx);
        cullingPassedCommandBuffer.Append(commandBuffer.Load(gCullMeshOffset + dtid));
    }
}