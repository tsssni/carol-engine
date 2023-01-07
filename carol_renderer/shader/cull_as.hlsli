#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

cbuffer CullConstants : register(b3)
{
    uint gViewCulledIdx;
    uint gOcclusionPassedIdx;
    uint gHiZIdx;
    uint gWidth;
    uint gHeight;
#ifdef SHADOW
    uint gLightIdx;
#endif
}

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{ 
    bool visible = GetMark(dtid, gOcclusionPassedIdx);
    
    if (dtid < gMeshletCount && !visible && !GetMark(dtid, gViewCulledIdx))
    {
#ifdef SHADOW
        float4x4 worldViewProj = mul(gWorld, gLights[gLightIdx].ViewProj);
#else
        float4x4 worldViewProj = mul(gWorld, gViewProj);
#endif
        StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataIdx];
        CullData cd = cullData[dtid];
            
        visible = (AabbFrustumTest(cd.Center, cd.Extents, worldViewProj) != OUTSIDE) && NormalConeTest(cd.Center, cd.NormalCone, cd.ApexOffset, gWorld, gEyePosW);       

        if(!visible)
        {
            SetMark(dtid, gViewCulledIdx);
        }
        else
        {
            visible = HiZOcclusionTest(cd.Center, cd.Extents, float2(gWidth, gHeight), worldViewProj, gHiZIdx);

            if(visible)
            {
                SetMark(dtid, gOcclusionPassedIdx);
            }
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