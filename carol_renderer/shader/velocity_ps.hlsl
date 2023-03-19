#include "include/common.hlsli"

struct PixelIn
{
    float4 PosH : SV_POSITION;
    float4 PosHist : POSITION;
};

float2 main(PixelIn pin) : SV_Target
{
    pin.PosHist /= pin.PosHist.w;
    
    float2 histPos;
    histPos.x = (pin.PosHist.x + 1.0f) / 2.0f;
    histPos.y = (1.0f - pin.PosHist.y) / 2.0f;
    histPos *= gRenderTargetSize;
    
    return histPos - pin.PosH.xy;
}