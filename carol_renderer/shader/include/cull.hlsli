#define THREADS_PER_WAVE 32
#define AS_GROUP_SIZE THREADS_PER_WAVE
#define INSIDE 0
#define OUTSIDE 1
#define INTERSECTING 2

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

bool IsConeDegenerate(CullData c)
{
    return (c.NormalCone >> 24) == 0xff;
}

float4 UnpackCone(uint packed)
{
    float4 v;
    v.x = float((packed >> 0) & 0xFF);
    v.y = float((packed >> 8) & 0xFF);
    v.z = float((packed >> 16) & 0xFF);
    v.w = float((packed >> 24) & 0xFF);

    v = v / 255.0;
    v.xyz = v.xyz * 2.0 - 1.0;

    return v;
}

bool NormalConeTest(CullData c, float4x4 world, float3 viewPos)
{
    if (IsConeDegenerate(c))
        return true;

    float4 normalCone = UnpackCone(c.NormalCone);
    float3 axis = normalize(mul(float4(normalCone.xyz, 0.f), world)).xyz;

    float3 apex = mul(float4(c.Center - normalCone.xyz * c.ApexOffset, 1.f), world).xyz;
    float3 view = normalize(viewPos - apex);

    if (dot(view, -axis) > normalCone.w)
    {
        return false;
    }

    return true;
}