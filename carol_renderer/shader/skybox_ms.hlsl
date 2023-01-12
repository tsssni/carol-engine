#include "include/common.hlsli"
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
	StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[gMeshletBufferIdx];
    Meshlet meshlet = meshlets[gid];
    
    SetMeshOutputCounts(meshlet.VertexCount, meshlet.PrimCount);
    
    if (gtid < meshlet.PrimCount)
    {
        tris[gtid] = UnpackPrim(meshlet.Prims[gtid]);
    }
    
    if (gtid < meshlet.VertexCount)
    {
        StructuredBuffer<MeshIn> vertices = ResourceDescriptorHeap[gVertexBufferIdx];
        MeshIn min = vertices[meshlet.Vertices[gtid]];
        MeshOut mout;
	
        mout.PosL = min.PosL;
        float4 posW = float4(min.PosL, 1.0f);
        posW.xyz += gEyePosW;
        mout.PosH = mul(posW, gViewProj).xyww;
        
        verts[gtid] = mout;
    }
}
