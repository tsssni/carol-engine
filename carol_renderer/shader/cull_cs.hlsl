#include "include/cull.hlsli"
#include "include/common.hlsli"
#include "include/mesh.hlsli"

struct IndirectCommand
{
    uint2 MeshCBAddr;
    uint2 SkinnedCBAddr;
    uint3 DispathMeshArgs;
};

struct MeshConstants
{
    float4x4 World;
    float4x4 HistWorld;
    
    float3 Center;
    float MeshPad0;
    float3 Extents;
    float MeshPad1;
    
    uint MeshletCount;
    uint VertexBufferIdx;
    uint MeshletBufferIdx;
    uint CullDataBufferIdx;
    
    uint MeshletFrustumCulledMarkBufferIdx;
    uint MeshletNormalConeCulledMarkBufferIdx;
    uint MeshletOcclusionCulledMarkBufferIdx;
    uint MeshletCulledMarkBufferIdx;
    
    uint MeshDiffuseMapIdx;
    uint MeshNormalMapIdx;
    uint MeshMetallicMapIdx;
    float MeshPad3;

    // Padding for 256 byte
    float4 MeshConstantsPad0;
    float4 MeshConstantsPad1;
    float4 MeshConstantsPad2;
};

bool InstanceFrustumCull(uint dtid, MeshConstants mc)
{
    bool culled = GetMark(gMeshOffset + dtid, gInstanceFrustumCulledMarkBufferIdx);

    if (gIteration == 0)
    {
#ifdef SHADOW
        float4x4 frustumWorldViewProj = mul(mc.World, gMainLights[gLightIdx].ViewProj);
#else
        float4x4 frustumWorldViewProj = mul(mc.World, gViewProj);
#endif
        
        if (AabbFrustumTest(mc.Center, mc.Extents, frustumWorldViewProj) == OUTSIDE)
        {
            SetMark(gMeshOffset + dtid, gInstanceFrustumCulledMarkBufferIdx);
            culled = true;
        }
    }

    return culled;
}

bool InstanceHiZOcclusionCull(uint dtid, MeshConstants mc)
{
    bool culled = GetMark(gMeshOffset + dtid, gInstanceOcclusionCulledMarkBufferIdx);

    if (culled)
    {

#ifdef SHADOW
        float4x4 occlusionWorldViewProj = mul(gIteration == 0 ? mc.HistWorld : mc.World, gMainLights[gLightIdx].ViewProj);
#else 
        float4x4 occlusionWorldViewProj = mul(gIteration == 0 ? mc.HistWorld : mc.World, gIteration == 0 ? gHistViewProj : gViewProj);
#endif
        
        if(HiZOcclusionTest(mc.Center, mc.Extents, occlusionWorldViewProj, gHiZIdx))
        {
            ResetMark(gMeshOffset + dtid, gInstanceOcclusionCulledMarkBufferIdx);
            culled = false;
        }
    }

    return culled;
}

[numthreads(32, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    if(dtid >= gMeshCount || !GetMark(gMeshOffset + dtid, gInstanceCulledMarkBufferIdx))
    {
        return;
    }
    
    bool culled = false;
    StructuredBuffer<MeshConstants> meshCB = ResourceDescriptorHeap[gMeshBufferIdx];
    MeshConstants mc = meshCB.Load(gMeshOffset + dtid);

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
    culled = false;
    if(!culled)
    {
        StructuredBuffer<IndirectCommand> commandBuffer = ResourceDescriptorHeap[gCommandBufferIdx];
        AppendStructuredBuffer<IndirectCommand> cullingPassedCommandBuffer = ResourceDescriptorHeap[gCullingPassedCommandBufferIdx];

        ResetMark(dtid, gInstanceCulledMarkBufferIdx);
        cullingPassedCommandBuffer.Append(commandBuffer.Load(gMeshOffset + dtid));
    }
}