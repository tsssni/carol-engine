#ifndef CULL_HEADER
#define CULL_HEADER

#define THREADS_PER_WAVE 32
#define AS_GROUP_SIZE THREADS_PER_WAVE

#define INSIDE 0
#define OUTSIDE 1
#define INTERSECTING 2

#include "common.hlsli"

cbuffer CullCB : register(b2)
{
    float4x4 gCullViewProj;
    float4x4 gCullHistViewProj;
    float3 gCullEyePos;
    uint gCullMeshCount;
    
    uint gCullPassedCommandBufferIdx;
    uint gCommandBufferIdx;
    uint gMeshBufferIdx;

    uint gInstanceFrustumCulledMarkBufferIdx;
    uint gInstanceOcclusionCulledMarkBufferIdx;
    uint gInstanceCulledMarkBufferIdx;
    
    uint gCullHiZMapIdx;
    float gCullPad0;
}

struct CullData
{
    float3 Center;
    float3 Extents;
    uint NormalCone;
    float ApexOffset;
};

struct Payload
{
    uint MeshletIndices[AS_GROUP_SIZE];
};

struct IndirectCommand
{
    uint2 MeshCBAddr;
    uint2 SkinnedCBAddr;
    uint3 DispathMeshArgs;
};

struct MeshConstants
{
    float4x4 World;
    float4x4 HistWorld;
    
    float3 Center;
    float MeshPad0;
    float3 Extents;
    float MeshPad1;
    
    uint MeshletCount;
    uint VertexBufferIdx;
    uint MeshletBufferIdx;
    uint CullDataBufferIdx;
    
    uint MeshletFrustumCulledMarkBufferIdx;
    uint MeshletNormalConeCulledMarkBufferIdx;
    uint MeshletOcclusionCulledMarkBufferIdx;
    uint MeshletCulledMarkBufferIdx;
    
    uint MeshDiffuseMapIdx;
    uint MeshNormalMapIdx;
    uint MeshMetallicMapIdx;
    float MeshPad3;

    // Padding for 256 byte
    float4 MeshConstantsPad0;
    float4 MeshConstantsPad1;
    float4 MeshConstantsPad2;
};

bool GetMark(uint idx, uint markIdx)
{
    RWByteAddressBuffer mark = ResourceDescriptorHeap[markIdx];
    uint byte = mark.Load(idx / 32u * 4);
    return (byte >> (idx % 32u)) & 1;
}

void SetMark(uint idx, uint markIdx)
{
    RWByteAddressBuffer mark = ResourceDescriptorHeap[markIdx];
    mark.InterlockedOr(idx / 32u * 4u, 1u << (idx % 32u));
}

void ResetMark(uint idx, uint markIdx)
{
    RWByteAddressBuffer mark = ResourceDescriptorHeap[markIdx];
    mark.InterlockedAnd(idx / 32u * 4u, ~(1u << (idx % 32u)));
}

uint AabbPlaneTest(float3 center, float3 extents, float4 plane)
{
    // The plane equation is plane.x*x+plane.y*y+plane.z*z+plane.w=0
    // Do the normalization for the plane to get the plane normal
    // Plane/AABB intersection algorithm reference: Real-Time Rendering 4th Edition, p970
    
    plane /= length(plane.xyz);
    float3 n = plane.xyz;
    float d = plane.w;
    
    float s = dot(center, n) + d;
    float e = dot(extents, abs(n));
    
    if (s + e < 0)
    {
        return INSIDE;
    }
    else if (s - e > 0)
    {
        return OUTSIDE;
    }
    else
    {
        return INTERSECTING;
    }

}

uint AabbFrustumTest(float3 center, float3 extents, float4x4 M)
{
    // Test against six planes to get the result
    // The planes are derived from the transformation matrix
    
    uint left = AabbPlaneTest(center, extents, float4(-(M._11 + M._14), -(M._21 + M._24), -(M._31 + M._34), -(M._41 + M._44)));
    if (left == OUTSIDE)
    {
        return OUTSIDE;
    }
    
    uint right = AabbPlaneTest(center, extents, float4(M._11 - M._14, M._21 - M._24, M._31 - M._34, M._41 - M._44));
    if (right == OUTSIDE)
    {
        return OUTSIDE;
    }
    
    uint up = AabbPlaneTest(center, extents, float4(-(M._12 + M._14), -(M._22 + M._24), -(M._32 + M._34), -(M._42 + M._44)));
    if(up == OUTSIDE)
    {
        return OUTSIDE;
    }
    
    uint down = AabbPlaneTest(center, extents, float4(M._12 - M._14, M._22 - M._24, M._32 - M._34, M._42 - M._44));
    if(down == OUTSIDE)
    {
        return OUTSIDE;
    }
    
    uint near = AabbPlaneTest(center, extents, float4(-M._13, -M._23, -M._33, -M._43));
    if(near == OUTSIDE)
    {
        return OUTSIDE;
    }
    
    uint far = AabbPlaneTest(center, extents, float4(M._13 - M._14, M._23 - M._24, M._33 - M._34, M._43 - M._44));
    if(far == OUTSIDE)
    {
        return OUTSIDE;
    }
    
    if(left == INSIDE && right == INSIDE && up == INSIDE && down == INSIDE && near == INSIDE && far == INSIDE)
    {
        return INSIDE;
    }
    else
    {
        return INTERSECTING;
    }
}

bool IsConeDegenerate(uint packedNormalCone)
{
    return (packedNormalCone >> 24) == 0xff;
}

float4 UnpackCone(uint packedNormalCone)
{
    float4 normalCone;
    normalCone.x = float((packedNormalCone >> 0) & 0xff);
    normalCone.y = float((packedNormalCone >> 8) & 0xff);
    normalCone.z = float((packedNormalCone >> 16) & 0xff);
    normalCone.w = float((packedNormalCone >> 24) & 0xff);

    normalCone = normalCone / 255.f;
    normalCone.xyz = normalCone.xyz * 2.f - 1.f;

    return normalCone;
}

bool NormalConeTest(float3 center, uint packedNormalCone, float apexOffset, float4x4 world, float3 viewPos)
{
    if (IsConeDegenerate(packedNormalCone))
        return true;

    float4 normalCone = UnpackCone(packedNormalCone);
    float3 axis = normalize(mul(float4(normalCone.xyz, 0.f), world)).xyz;

    float3 apex = mul(float4(center - normalCone.xyz * apexOffset, 1.f), world).xyz;
    float3 view = normalize(viewPos - apex);

    if (dot(view, -axis) > normalCone.w)
    {
        return false;
    }

    return true;
}

bool HiZOcclusionTest(float3 center, float3 extents, float4x4 M, uint hiZIdx)
{
    Texture2D hiZMap = ResourceDescriptorHeap[hiZIdx];
    uint2 size;
    uint maxMipLevel;

    hiZMap.GetDimensions(0, size.x, size.y, maxMipLevel);
    maxMipLevel -= 1;
    
    static float dx[8] = { -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f };
    static float dy[8] = { -1.f, -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f };
    static float dz[8] = { -1.f, -1.f, -1.f, -1.f, 1.f, 1.f, 1.f, 1.f };
    
    float4 v[8];
    float2 boxMin = { 1.f, 1.f };
    float2 boxMax = { 0.f, 0.f };
    float minZ = 1.f;
    
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        v[i] = float4(center + extents * float3(dx[i], dy[i], dz[i]), 1.f);
        v[i] = mul(v[i], M);
        v[i] /= v[i].w;
        v[i].xy = v[i].xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
        
        if (v[i].x < 0.f || v[i].x >= 1.f || v[i].y < 0.f || v[i].y >= 1.f || v[i].z < 0.f || v[i].z > 1.f)
        {
            return true;
        }
        
        boxMin = min(boxMin, v[i].xy);
        boxMax = max(boxMax, v[i].xy);
        minZ = saturate(min(minZ, v[i].z));
    }

    float4 box = float4(boxMin, boxMax);
    float2 boxSize = (boxMax - boxMin) * size;
    float lod = clamp(log2(max(boxSize.x, boxSize.y)), 0, maxMipLevel);
    
    float4 depth;
    depth.x = hiZMap.SampleLevel(gsamLinearClamp, box.xy, lod).r;
    depth.y = hiZMap.SampleLevel(gsamLinearClamp, box.zy, lod).r;
    depth.z = hiZMap.SampleLevel(gsamLinearClamp, box.xw, lod).r;
    depth.w = hiZMap.SampleLevel(gsamLinearClamp, box.zw, lod).r;
    
    float maxDepth = max(depth.x, max(depth.y, max(depth.z, depth.w)));
    return minZ < maxDepth;
}

#endif