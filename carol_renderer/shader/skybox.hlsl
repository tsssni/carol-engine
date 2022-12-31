#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

struct VertexOut
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
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
        VertexOut vout;
	
        vout.PosL = vin.PosL;
        float4 posW = float4(vin.PosL, 1.0f);
        posW.xyz += gEyePosW;
        vout.PosH = mul(posW, gViewProj).xyww;
        
        verts[gtid] = vout;
    }
}

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	vout.PosL = vin.PosL;
    float4 posW = float4(vin.PosL, 1.0f);
	posW.xyz += gEyePosW;
	vout.PosH = mul(posW, gViewProj).xyww;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	TextureCube gSkyBox = ResourceDescriptorHeap[gDiffuseMapIdx];
    return gSkyBox.Sample(gsamLinearWrap, pin.PosL);
}