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
    
    float3 FresnelR0;
    float Roughness;
    
    uint MeshletCount;
    uint VertexBufferIdx;
    uint MeshletBufferIdx;
    uint CullDataBufferIdx;
    
    uint FrustumCulledMarkBufferIdx;
    uint OcclusionPassedMarkBufferIdx;
    
    uint MeshDiffuseMapIdx;
    uint MeshNormalMapIdx;
    
    float4 ConstantPad0;
    float4 ConstantPad1;
    float4 ConstantPad2;
};


[numthreads(32, 1, 1)]
void main( uint dtid : SV_DispatchThreadID )
{
#ifdef OCCLUSION
    if (dtid < gMeshCount && !GetMark(dtid, gInstanceFrustumCulledMarkIdx) && !GetMark(dtid, gInstanceOcclusionPassedMarkIdx))
#else
    if (dtid < gMeshCount && !GetMark(dtid, gInstanceFrustumCulledMarkIdx))
#endif
    {
        StructuredBuffer<IndirectCommand> commandBuffer = ResourceDescriptorHeap[gCommandBufferIdx];
        AppendStructuredBuffer<IndirectCommand> cullCommandBuffer = ResourceDescriptorHeap[gCullCommandBufferIdx];
        StructuredBuffer<MeshConstants> meshCB = ResourceDescriptorHeap[gMeshCBIdx];
        
        MeshConstants mc = meshCB.Load(gMeshOffset + dtid);

#ifdef SHADOW
        float4x4 frustumWorldViewProj = mul(mc.World, gLights[gLightIdx].ViewProj);
#ifdef OCCLUSION
        float4x4 occlusionWorldViewProj = mul(gIsHist ? mc.HistWorld : mc.World, gLights[gLightIdx].ViewProj);
#endif
#else 
        float4x4 frustumWorldViewProj = mul(mc.World, gViewProj);
#ifdef OCCLUSION
        float4x4 occlusionWorldViewProj = mul(gIsHist ? mc.HistWorld : mc.World, gIsHist ? gHistViewProj : gViewProj);
#endif
#endif
        
        if (AabbFrustumTest(mc.Center, mc.Extents, frustumWorldViewProj) == OUTSIDE)
        {
            SetMark(gMeshOffset + dtid, gInstanceFrustumCulledMarkIdx);
        }
#ifdef OCCLUSION
        else if(!HiZOcclusionTest(mc.Center, mc.Extents, occlusionWorldViewProj, gHiZIdx))
        {
            SetMark(gMeshOffset + dtid, gInstanceOcclusionPassedMarkIdx);
            cullCommandBuffer.Append(commandBuffer.Load(gMeshOffset + dtid)); 
        }
#else
        else
        {
            cullCommandBuffer.Append(commandBuffer.Load(gMeshOffset + dtid)); 
        }
#endif
    }
}