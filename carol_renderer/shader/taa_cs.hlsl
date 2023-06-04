#define BORDER_RADIUS 1
#include "include/common.hlsli"
#include "include/compute.hlsli"
#include "include/texture.hlsli"

#define SAMPLE_COUNT 9

static const int2 delta[9] =
{
    int2(-1, -1),int2(-1, 0),int2(-1, 1),
    int2(0, -1),int2(0, 0),int2(0, 1),
    int2(1, -1),int2(1, 0),int2(1, 1)
};

groupshared float3 color[32][32];
groupshared float depth[32][32];
groupshared float2 velocity[32][32];

float3 Rgb2Ycocg(float3 rgbColor)
{
    float3 ycocg;
    
    ycocg.x = 0.25f * rgbColor.x + 0.5f * rgbColor.y + 0.25 * rgbColor.z;
    ycocg.y = 0.5f * rgbColor.x + (-0.5f) * rgbColor.z;
    ycocg.z = -0.25f * rgbColor.x + 0.5f * rgbColor.y + (-0.25) * rgbColor.z;
    
    return ycocg;
}

float3 Ycocg2Rgb(float3 ycocgColor)
{
    float3 rgb;
    
    float temp = ycocgColor.x - ycocgColor.z;
    rgb.r = temp + ycocgColor.y;
    rgb.g = ycocgColor.x + ycocgColor.z;
    rgb.b = temp - ycocgColor.y;
    
    return rgb;
}

float3 Clip(float3 histColor, float3 minPixelColor, float3 maxPixelColor)
{
    float3 centerColor = (minPixelColor + maxPixelColor) / 2;
    float3 ray = centerColor - histColor;
    float3 invRay = rcp(ray);
    
    float3 minRay = (minPixelColor - histColor) * invRay;
    float3 maxRay = (maxPixelColor - histColor) * invRay;
    float3 factorVec = min(minRay, maxRay);
    
    float blendFactor = saturate(max(max(factorVec.x, factorVec.y), factorVec.z));
    return lerp(histColor, centerColor, blendFactor);
}


void CalcPixelColorAabb(int2 pos, inout float3 minPixelColor, inout float3 maxPixelColor)
{
    minPixelColor = float3(1.0f, 0.5f, 0.5f);
    maxPixelColor = float3(0.0f, -0.5f, -0.5f);

    float3 meanColor = float3(0.0f, 0.0f, 0.0f);
    float3 varColor = float3(0.0f, 0.0f, 0.0f);
    const static float3 gamma = 1.0f;
    
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float3 pixelColor = Rgb2Ycocg(color[pos.x+delta[i].x][pos.y+delta[i].y].rgb);
        meanColor += pixelColor;
        varColor += pixelColor * pixelColor;
    }

    meanColor /= SAMPLE_COUNT;
    varColor = sqrt(varColor / SAMPLE_COUNT - meanColor * meanColor);
    
    minPixelColor = meanColor - gamma * varColor;
    maxPixelColor = meanColor + gamma * varColor;
}

[numthreads(32, 32, 1)]
void main(int2 gid : SV_GroupID, int2 gtid : SV_GroupThreadID)
{
    Texture2D depthMap = ResourceDescriptorHeap[gDepthStencilMapIdx];
    Texture2D velocityMap = ResourceDescriptorHeap[gVelocityMapIdx];
    RWTexture2D<float4> frameMap = ResourceDescriptorHeap[gRWFrameMapIdx];
    RWTexture2D<float4> histMap = ResourceDescriptorHeap[gRWHistMapIdx];
    Texture2D histFrameMap = ResourceDescriptorHeap[gHistMapIdx];

    int2 uid = GetUavId(gid, gtid, BORDER_RADIUS);
    depth[gtid.x][gtid.y] = TextureBorderTest(uid, gRenderTargetSize) ? depthMap[uid].r : 1.f;
    color[gtid.x][gtid.y] = frameMap[uid].rgb;
    velocity[gtid.x][gtid.y] = velocityMap[uid].rg;
    GroupMemoryBarrierWithGroupSync();

    float minZ = 1.0f;
    int2 minZPos = gtid;
    
    if (GroupBorderTest(gtid, BORDER_RADIUS) && TextureBorderTest(uid, gRenderTargetSize))
    {
        [unroll]
        for (int i = 0; i < SAMPLE_COUNT; ++i)
        {
            int2 adjacentPos = gtid + delta[i];
            float adjacentZ = depth[adjacentPos.x][adjacentPos.y];
        
            if (adjacentZ < minZ)
            {
                minZ = adjacentZ;
                minZPos = adjacentPos;
            }
        }
    
        int2 framePos = gtid;
        float2 histPos = (uid + .5f + velocity[minZPos.x][minZPos.y]) * gInvRenderTargetSize;
        
        float3 framePixelColor = color[gtid.x][gtid.y];
        float3 histPixelColor = histFrameMap.Sample(gsamLinearClamp, histPos).rgb;
    
        float3 minPixelColor;
        float3 maxPixelColor;
        CalcPixelColorAabb(framePos, minPixelColor, maxPixelColor);
        histPixelColor = Ycocg2Rgb(Clip(Rgb2Ycocg(histPixelColor), minPixelColor, maxPixelColor));
    
        float4 taaPixelColor = float4(0.05f * framePixelColor + 0.95f * histPixelColor, 1.f);
        frameMap[uid] = taaPixelColor;
        histMap[uid] = taaPixelColor;
    }
}