#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
};

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
        vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
        vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
        vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
        
        verts[gtid] = vout;
    }
}

float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    float3 normalV = mul(float4(pin.NormalW, 0.0f), gView).xyz;
    return float4(normalV, 0.0f);
}
