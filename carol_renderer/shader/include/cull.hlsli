#define THREADS_PER_WAVE 32
#define AS_GROUP_SIZE THREADS_PER_WAVE
#define INSIDE 0
#define OUTSIDE 1
#define INTERSECTING 2

struct Payload
{
    uint MeshletIndices[AS_GROUP_SIZE];
};

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