#include "include/screen.hlsli"

struct MeshOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

[numthreads(6, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[2],
    out vertices MeshOut verts[6])
{   
    SetMeshOutputCounts(6, 2);
    
    if (gtid == 0)
    {
        tris[gtid] = uint3(0, 1, 2);
    }
    else if (gtid == 1)
    {
        tris[gtid] = uint3(3, 4, 5);
    }
    
    if (gtid < 6)
    {
        MeshOut mout;

        mout.TexC = gTexCoords[gtid];
        mout.PosH = float4(2.0f * mout.TexC.x - 1.0f, 1.0f - 2.0f * mout.TexC.y, 0.0f, 1.0f);
        verts[gtid] = mout;         
    }
}