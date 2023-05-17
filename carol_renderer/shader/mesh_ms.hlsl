#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"
#include "include/texture.hlsli"

struct MeshOut
{
    float4 PosH : SV_POSITION;
    float4 PosVelo : POSITION0;
    float4 PosHist : POSITION1;
    float3 PosW : POSITION2;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD; 
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
        float3 posL = min.PosL;
        min = SkinnedTransform(min);
#endif

        MeshOut mout;

        mout.PosW = mul(float4(min.PosL, 1.0f), gWorld).xyz;
        mout.NormalW = normalize(mul(min.NormalL, (float3x3) gWorld));
        mout.TangentW = normalize(mul(min.TangentL, (float3x3) gWorld));
        mout.TexC = min.TexC;
        
#ifdef SKINNED
        min.PosL = HistSkinnedTransformPosL(posL, min);
#endif
        float3 posWHist = mul(float4(min.PosL, 1.f), gHistWorld).xyz;

        mout.PosH = mul(float4(mout.PosW, 1.0f), gViewProj);
        mout.PosVelo = mul(float4(mout.PosW, 1.0f), gVeloViewProj);
        mout.PosHist = mul(float4(posWHist, 1.f), gHistViewProj);

        verts[gtid] = mout;
    }
}