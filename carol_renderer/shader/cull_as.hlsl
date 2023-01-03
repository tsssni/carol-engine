#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{ 
    bool visible = false;
    
    if (dtid < gMeshletCount)
    {
        float4x4 worldViewProj = mul(gWorld, gViewProj);
        uint test = AabbFrustumTest(gCenter, gExtents, worldViewProj);

        StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataIdx];
        CullData cd = cullData[dtid];
        
        if (test == INSIDE)
        {
            visible = NormalConeTest(cd, gWorld, gEyePosW);
        }
        else if (test == INTERSECTING)
        {
            
            visible = (AabbFrustumTest(cd.Center, cd.Extents, worldViewProj) != OUTSIDE) && NormalConeTest(cd, gWorld, gEyePosW);
        }        
    }
    
    if (visible)
    {
        uint idx = WavePrefixCountBits(visible);
        sharedPayload.MeshletIndices[idx] = dtid;
    }
    
    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, sharedPayload);
}