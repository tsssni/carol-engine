#include "include/common.hlsli"
#include "include/mesh.hlsli"

struct PixelIn
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

float4 main(PixelIn pin) : SV_Target
{	
	TextureCube skyBox = ResourceDescriptorHeap[gDiffuseMapIdx];
    return skyBox.Sample(gsamLinearWrap, pin.PosL);
}