#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

struct MeshOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD; 
#ifdef SSAO
    float4 SsaoPosH : POSITION1;
#endif

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
    StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[gMeshletIdx];
    Meshlet m = meshlets[meshletIdx];
    
    SetMeshOutputCounts(m.VertexCount, m.PrimCount);
    
    if (gtid < m.PrimCount)
    {
        tris[gtid] = UnpackPrim(m.Prims[gtid]);
    }
    
    if (gtid < m.VertexCount)
    {
        StructuredBuffer<MeshIn> vertices = ResourceDescriptorHeap[gVertexIdx];
        MeshIn min = vertices[m.Vertices[gtid]];
            
#ifdef SKINNED
        min = SkinnedTransform(min);
#endif

        MeshOut mout;

        mout.PosW = mul(float4(min.PosL, 1.0f), gWorld).xyz;
        mout.NormalW = normalize(mul(min.NormalL, (float3x3) gWorld));
        mout.TangentW = normalize(mul(min.TangentL, (float3x3) gWorld));
        mout.TexC = min.TexC;
       
#ifdef TAA
        mout.PosH = mul(float4(mout.PosW, 1.0f), gJitteredViewProj);
#else
        mout.PosH = mul(float4(mout.PosW, 1.0f), gViewProj);
#endif

#ifdef SSAO
        mout.SsaoPosH = mul(float4(mout.PosW, 1.0f), gViewProjTex);
#endif
        
        verts[gtid] = mout;
    }
}