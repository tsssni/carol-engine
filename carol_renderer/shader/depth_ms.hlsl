#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

struct MeshOut
{
    float4 PosH : SV_POSITION;
};

[numthreads(128, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload Payload payload,
    out indices uint3 tris[126],
    out vertices MeshOut verts[64])
{
    uint meshletIdx = payload.MeshletIndices[gid];
    StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[gMeshletBufferIdx];
    Meshlet meshlet = meshlets[meshletIdx];
    
    StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataBufferIdx];
    CullData cd = cullData[1];
    float4x4 worldViewProj = mul(gWorld, gViewProj);
    
    SetMeshOutputCounts(meshlet.VertexCount, meshlet.PrimCount);
    
    if (gtid < meshlet.PrimCount)
    {
        tris[gtid] = UnpackPrim(meshlet.Prims[gtid]);
    }
    
    if (gtid < meshlet.VertexCount)
    {
        StructuredBuffer<MeshIn> vertices = ResourceDescriptorHeap[gVertexBufferIdx];
        MeshIn min = vertices[meshlet.Vertices[gtid]];

#ifdef SKINNED
        min = SkinnedTransform(min);
#endif
        
        float4x4 worldViewProj;
        
#ifdef SHADOW
        worldViewProj = mul(gWorld, gMainLights[gLightIdx].ViewProj);
#else
        worldViewProj = mul(gWorld, gViewProj);
#endif
     
        MeshOut mout;
        mout.PosH = mul(float4(min.PosL, 1.0f), worldViewProj);
        
        verts[gtid] = mout;
    }
}

