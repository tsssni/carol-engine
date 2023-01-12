#include "include/common.hlsli"
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
        if(GetMark(dtid, gMeshletFrustumCulledMarkBufferIdx))
        {
            visible = false;
        }
#ifdef OCCLUSION
        else if (GetMark(dtid, gMeshletOcclusionPassedMarkBufferIdx))
        {
            visible = true;
        }
#endif
        else
        {    
#ifdef SHADOW
            float4x4 frustumWorldViewProj = mul(gWorld, gLights[gLightIdx].ViewProj);
#ifdef OCCLUSION
            float4x4 occlusionWorldViewProj = mul(gIsHist ? gHistWorld : gWorld, gLights[gLightIdx].ViewProj);
#endif
#else 
            float4x4 frustumWorldViewProj = mul(gWorld, gViewProj);
#ifdef OCCLUSION
            float4x4 occlusionWorldViewProj = mul(gIsHist ? gHistWorld : gWorld, gIsHist ? gHistViewProj : gViewProj);
#endif
#endif  

            StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataBufferIdx];
            CullData cd = cullData[dtid];
            
            if (AabbFrustumTest(cd.Center, cd.Extents, frustumWorldViewProj) != OUTSIDE && NormalConeTest(cd.Center, cd.NormalCone, cd.ApexOffset, gWorld, gEyePosW))
            {
#ifdef OCCLUSION
                if(!HiZOcclusionTest(cd.Center, cd.Extents, occlusionWorldViewProj, gHiZIdx))
                {
                    visible = true;
                    SetMark(dtid, gMeshletOcclusionPassedMarkBufferIdx);
                }
#else
                visible = true;
#endif
            }
            else
            {
                visible = false;
                SetMark(dtid, gMeshletFrustumCulledMarkBufferIdx);
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