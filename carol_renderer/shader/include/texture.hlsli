#ifndef TEXTURE_HEADER
#define TEXTURE_HEADER

float LOD(float depth)
{
    return clamp(pow(depth, 15.0f) * 8.0f, 0.f, 7.f);
}

#endif
