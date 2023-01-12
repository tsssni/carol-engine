#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

#ifdef SKINNED
float3 HistPosL(float3 posL, MeshIn min)
{    
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = min.BoneWeights.x;
    weights[1] = min.BoneWeights.y;
    weights[2] = min.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
    
    float3 histPosL = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        histPosL += weights[i] * mul(float4(posL, 1.0f), gHistBoneTransforms[min.BoneIndices[i]]).xyz;
    }
    
    return histPosL;
}
#endif

struct MeshOut
{
    float4 PosH : SV_POSITION;
    float4 PosHist : POSITION;
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
        float3 posL = min.PosL;
            
#ifdef SKINNED
        min = SkinnedTransform(min);
#endif

        MeshOut mout;

        float3 posW = mul(float4(min.PosL, 1.0f), gWorld).xyz;
        mout.PosH = mul(float4(posW, 1.0f), gViewProj);

#ifdef SKINNED
        min.PosL = HistPosL(posL, min);
#endif
    
        float3 posWHist = mul(float4(min.PosL, 1.0f), gHistWorld).xyz;
        mout.PosHist = mul(float4(posWHist, 1.0f), gHistViewProj);
        
        verts[gtid] = mout;
    }
}