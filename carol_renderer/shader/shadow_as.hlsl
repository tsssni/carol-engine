#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

cbuffer LightIdx : register(b3)
{
    uint gLightIdx;
}

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{ 
    bool visible = false;
    
    if (dtid < gMeshletCount)
    {
        float4x4 worldViewProj = mul(gWorld, gLights[gLightIdx].ViewProj);
        uint test = AabbFrustumTest(gCenter, gExtents, worldViewProj);
        
        if (test == INSIDE)
        {
            visible = true;
        }
        else if (test == INTERSECTING)
        {
            StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataIdx];
            CullData cd = cullData[dtid];
            visible = (AabbFrustumTest(cd.Center, cd.Extents, worldViewProj) != OUTSIDE);
        }
        
        if(visible)
        {
            uint idx = WavePrefixCountBits(visible);
            sharedPayload.MeshletIndices[idx] = dtid;
        }
    }

    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, sharedPayload);
}