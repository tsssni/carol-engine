#include "include/root_signature.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

Texture2D gDepthMap : register(t0);
Texture2D gCurrMap : register(t1);
Texture2D gHistMap : register(t2);
Texture2D gVelocityMap : register(t3);

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

static int sampleCount = 9;
static int dx[9] = { -1, -1, -1, 0, 0, 0, 1, 1, 1 };
static int dy[9] = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    float2 texC = gTexCoords[vid];
    
    vout.PosH = float4(2.0f * texC.x - 1.0f, 1.0f - 2.0f * texC.y, 0.0f, 1.0f);
    return vout;
}

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


void CalcPixelColorAabb(float2 pixelPos, inout float3 minPixelColor, inout float3 maxPixelColor)
{
    minPixelColor = float3(1.0f, 0.5f, 0.5f);
    maxPixelColor = float3(0.0f, -0.5f, -0.5f);

    float3 meanColor = float3(0.0f, 0.0f, 0.0f);
    float3 varColor = float3(0.0f, 0.0f, 0.0f);
    static float3 gamma = 1.0f;
    
    [unroll]
    for (int i = 0; i < sampleCount; i++)
    {
        float2 pos = float2(pixelPos.x + dx[i], pixelPos.y + dy[i]) * gInvRenderTargetSize;
        float3 pixelColor = Rgb2Ycocg(gCurrMap.Sample(gsamPointClamp, pos).rgb);
        meanColor += pixelColor;
        varColor += pixelColor * pixelColor;
    }

    meanColor /= sampleCount;
    varColor = sqrt(abs(varColor / sampleCount - meanColor * meanColor));
    
    minPixelColor = meanColor - gamma * varColor;
    maxPixelColor = meanColor + gamma * varColor;
}

float4 PS(VertexOut pin) : SV_Target
{
    float minZ = 1.0f;
    float2 minZPos = pin.PosH.xy * gInvRenderTargetSize;
    
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 adjacentPos = int2(pin.PosH.x + dx[i], pin.PosH.y + dy[i]) * gInvRenderTargetSize;
        float adjacentZ = gDepthMap.Sample(gsamPointClamp, adjacentPos).r;
        
        if (adjacentZ < minZ)
        {
            minZ = adjacentZ;
            minZPos = adjacentPos;
        }
    }
    
    float2 currPos = pin.PosH.xy * gInvRenderTargetSize;
    float2 histPos = currPos + gVelocityMap.Sample(gsamPointClamp, minZPos).xy;
        
    float4 currPixelColor = gHistMap.Sample(gsamPointClamp, currPos);
    float4 histPixelColor = gCurrMap.Sample(gsamPointClamp, histPos);
    
    float3 minPixelColor;
    float3 maxPixelColor;
    CalcPixelColorAabb(pin.PosH.xy, minPixelColor, maxPixelColor);
    
    histPixelColor.rgb = Ycocg2Rgb(Clip(Rgb2Ycocg(histPixelColor.rgb), minPixelColor, maxPixelColor));
    
    float4 taaPixel = 0.05f * currPixelColor + 0.95f * histPixelColor;
    return taaPixel;
}
