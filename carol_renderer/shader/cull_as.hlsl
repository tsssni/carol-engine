#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

bool MeshletFrustumCull(uint dtid, CullData cd)
{
    bool culled = false;

    if(gIteration == 0)
    {
    
#ifdef SHADOW
        float4x4 frustumWorldViewProj = mul(gWorld, gMainLights[gLightIdx].ViewProj);
#else 
        float4x4 frustumWorldViewProj = mul(gWorld, gViewProj);
#endif  
        if (AabbFrustumTest(cd.Center, cd.Extents, frustumWorldViewProj) == OUTSIDE)
        {
            SetMark(dtid, gMeshletFrustumCulledMarkBufferIdx);
            culled = true;
        }
    }
    else
    {
        culled = GetMark(dtid, gMeshletFrustumCulledMarkBufferIdx);
    }

    return culled;
}

bool MeshletNormalConeCull(uint dtid, CullData cd)
{
    bool culled = false;

    if(gIteration == 0)
    {
        if (!NormalConeTest(cd.Center, cd.NormalCone, cd.ApexOffset, gWorld, gEyePosW))
        {
            SetMark(dtid, gMeshletNormalConeCulledMarkBufferIdx);
            culled = true;
        }
    }
    else
    {
        culled = GetMark(dtid, gMeshletNormalConeCulledMarkBufferIdx);
    }
    
    return culled;
}

bool MeshletHiZOcclusionCull(uint dtid, CullData cd)
{
    bool culled = GetMark(dtid, gMeshletOcclusionCulledMarkBufferIdx);

    if(culled)
    {
#ifdef SHADOW
        float4x4 occlusionWorldViewProj = mul(gIteration == 0 ? gHistWorld : gWorld, gMainLights[gLightIdx].ViewProj);
#else 
        float4x4 occlusionWorldViewProj = mul(gIteration == 0 ? gHistWorld : gWorld, gIteration == 0 ? gHistViewProj : gViewProj);
#endif

        if (HiZOcclusionTest(cd.Center, cd.Extents, occlusionWorldViewProj, gHiZIdx))
        {
            ResetMark(dtid, gMeshletNormalConeCulledMarkBufferIdx);
            culled = false;
        }
    }

    return culled;
}

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    bool visible = dtid < gMeshletCount && !GetMark(dtid, gMeshletCulledMarkBufferIdx);

#ifdef WRITE
    if(dtid >= gMeshletCount || visible)
    {
        visible = false;
    }
    else
    {
        bool culled = false;
        StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataBufferIdx];
        CullData cd = cullData[dtid];

    #ifdef FRUSTUM
        if(!culled)
        {
            culled |= MeshletFrustumCull(dtid, cd);
        }
    #endif
    #ifdef NORMAL_CONE
        if(!culled)
        {
            culled |= MeshletNormalConeCull(dtid, cd);
        }
    #endif
    #ifdef HIZ_OCCLUSION
        if(!culled)
        {
            culled |= MeshletHiZOcclusionCull(dtid, cd);
        }
    #endif
        visible = !culled;
    }
#endif 

#if (defined WRITE) && (defined TRANSPARENT)
    DispatchMesh(0, 0, 0, sharedPayload);
#else
    if (visible)
    {
#ifdef WRITE
        ResetMark(dtid, gMeshletCulledMarkBufferIdx);
#endif
        uint idx = WavePrefixCountBits(visible);
        sharedPayload.MeshletIndices[idx] = dtid;
    }
    
    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, sharedPayload);
#endif
}