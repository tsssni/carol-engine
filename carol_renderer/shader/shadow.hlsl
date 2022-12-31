#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

cbuffer LightIdx : register(b3)
{
    uint gLightIdx;
}

[numthreads(128, 1, 1)]
[OutputTopology("triangle")]
void MS(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[126],
    out vertices VertexOut verts[64])
{
    StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[gMeshletIdx];
    Meshlet m = meshlets[gid];
    
    SetMeshOutputCounts(m.VertexCount, m.PrimCount);
    
    if (gtid < m.PrimCount)
    {
        tris[gtid] = UnpackPrim(m.Prims[gtid]);
    }
    
    if (gtid < m.VertexCount)
    {
        StructuredBuffer<VertexIn> vertices = ResourceDescriptorHeap[gVertexIdx];
        VertexIn vin = vertices[m.Vertices[gtid]];

#ifdef SKINNED
        vin = SkinnedTransform(vin);
#endif
     
        VertexOut vout;
        float3 posW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
        vout.PosH = mul(float4(posW, 1.0f), gLights[gLightIdx].ViewProj);
        
        verts[gtid] = vout;
    }
}

