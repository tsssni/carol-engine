#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"

struct PixelIn
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

float4 main(PixelIn pin) : SV_Target
{	
	TextureCube gSkyBox = ResourceDescriptorHeap[gDiffuseMapIdx];
    return gSkyBox.Sample(gsamLinearWrap, pin.PosL);
}