#include "Common.hlsl"

Texture2D gDepthMap : register(t0);
Texture2D gHistFrameMap : register(t1);
Texture2D gCurrFrameMap : register(t2);
Texture2D gVelocityMap : register(t3);

struct VertexOut
{
    float4 PosH : SV_POSITION;
};

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

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

float3 Clip(float3 histColor, float3 currColor, float3 minPixelColor, float3 maxPixelColor)
{
    float3 ray = currColor - histColor;
    float3 invRay = rcp(ray);
    
    float3 minRay = (minPixelColor - histColor) * invRay;
    float3 maxRay = (maxPixelColor - histColor) * invRay;
    float3 factorVec = min(minRay, maxRay);
    
    float blendFactor = saturate(max(factorVec.x, max(factorVec.y, factorVec.z)));
    return lerp(histColor, currColor, blendFactor);
}


void CalcPixelColorAabb(float2 currPos, inout float3 minPixelColor, inout float3 maxPixelColor)
{
    minPixelColor = float3(1.0f, 0.5f, 0.5f);
    maxPixelColor = float3(0.0f, -0.5f, -0.5f);
    
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        float2 pos = float2(currPos.x + dx[i], currPos.y + dy[i]) * gInvRenderTargetSize;
        float3 pixelColor = Rgb2Ycocg(gCurrFrameMap.Sample(gsamPointClamp, pos).rgb);
        minPixelColor = min(minPixelColor, pixelColor);
        maxPixelColor = max(maxPixelColor, pixelColor);
    }

}

float4 PS(VertexOut pin) : SV_Target
{
    float minZ = 1.0f;
    float2 minZPos = pin.PosH.xy;
    
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 adjacentPos = int2(pin.PosH.x + dx[i] * gInvRenderTargetSize.x, pin.PosH.y + dy[i] * gInvRenderTargetSize.y);
        
        float adjacentZ = gDepthMap.Sample(gsamPointClamp, adjacentPos).r;
        
        if (adjacentZ < minZ)
        {
            minZ = adjacentZ;
            minZPos = adjacentPos;
        }
    }
    
    float2 currPos = pin.PosH.xy * gInvRenderTargetSize;
    float2 histPos = currPos + gVelocityMap.Sample(gsamPointClamp, minZPos).xy;

    float4 currPixel = gCurrFrameMap.Sample(gsamPointClamp, currPos);
    float4 histPixel = gHistFrameMap.Sample(gsamPointClamp, histPos);
    
    float3 minPixelColor;
    float3 maxPixelColor;
    
    CalcPixelColorAabb(pin.PosH.xy, minPixelColor, maxPixelColor);
    
    histPixel.rgb = Ycocg2Rgb(Clip(Rgb2Ycocg(histPixel.rgb), Rgb2Ycocg(currPixel.rgb), minPixelColor, maxPixelColor));
    
    float4 taaPixel = 0.05f * currPixel + 0.95f * histPixel;
    return taaPixel;
}
