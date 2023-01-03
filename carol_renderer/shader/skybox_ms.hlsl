#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

struct MeshOut
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};


[numthreads(128, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[126],
    out vertices MeshOut verts[64])
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
        StructuredBuffer<MeshIn> vertices = ResourceDescriptorHeap[gVertexIdx];
        MeshIn min = vertices[m.Vertices[gtid]];
        MeshOut mout;
	
        mout.PosL = min.PosL;
        float4 posW = float4(min.PosL, 1.0f);
        posW.xyz += gEyePosW;
        mout.PosH = mul(posW, gViewProj).xyww;
        
        verts[gtid] = mout;
    }
}
