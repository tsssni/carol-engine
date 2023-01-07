#include "include/cull.hlsli"
#include "include/common.hlsli"

struct Mesh
{
    float4x4 World;
    float4x4 HistWorld;
    
    float3 Center;
    uint MeshletCount;
    float3 Extents;
    float MeshPad0;
    
    float3 FresnelR0;
    float Roughness;
};

struct IndirectCommand
{
    uint2 MeshCB;
    
    uint VertexIdx;
    uint MeshletIdx;
    uint CullDataIdx;
    uint CullMarkIdx;
    uint DiffuseMapIdx;
    uint NormalMapIdx;

    uint2 SkinnedCB;
    uint3 DispathMeshArgs;
};

cbuffer CullConstants : register(b3)
{
    uint gCommandBufferIdx;
    uint gCullCommandBufferIdx;
    uint gInstanceFrustumCulledMarkIdx;
    uint gInstanceOcclusionPassedMarkIdx;
    uint gHiZIdx;
    uint gMeshCBIdx;
    uint gMeshCount;
    uint gWidth;
    uint gHeight;
    
#ifdef SHADOW
    uint gLightIdx;
#endif
}

[numthreads(32, 1, 1)]
void main( uint dtid : SV_DispatchThreadID )
{
    if (dtid < gMeshCount && !GetMark(dtid, gInstanceFrustumCulledMarkIdx) && !GetMark(dtid, gInstanceOcclusionPassedMarkIdx))
    {
        RWStructuredBuffer<IndirectCommand> commandBuffer = ResourceDescriptorHeap[gCommandBufferIdx];
        AppendStructuredBuffer<IndirectCommand> cullCommandBuffer = ResourceDescriptorHeap[gCullCommandBufferIdx];
        StructuredBuffer<Mesh> meshCB = ResourceDescriptorHeap[gMeshCBIdx];
        
        Mesh mesh = meshCB.Load(dtid);
        float4x4 worldViewProj;
#ifdef SHADOW
        worldViewProj = mul(mesh.World, gLights[gLightIdx].ViewProj);
#else
        worldViewProj = mul(mesh.World, gViewProj);
#endif
        bool visible = AabbFrustumTest(mesh.Center, mesh.Extents, worldViewProj) != OUTSIDE;
        
        if (!visible)
        {
            SetMark(dtid, gInstanceFrustumCulledMarkIdx);
        }
        else
        {
            visible = HiZOcclusionTest(mesh.Center, mesh.Extents, float2(gWidth, gHeight), worldViewProj, gHiZIdx) != OUTSIDE;
            
            if(visible)
            {
                SetMark(dtid, gInstanceOcclusionPassedMarkIdx);
                cullCommandBuffer.Append(commandBuffer.Load(dtid));
            }  
        }
        
    }
}