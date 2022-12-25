#include "include/root_signature.hlsli"

struct VertexOut
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};
 
VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	vout.PosL = vin.PosL;
    float4 posW = float4(vin.PosL, 1.0f);
	posW.xyz += gEyePosW;
	vout.PosH = mul(posW, gViewProj).xyww;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return gTexCube[0].Sample(gsamLinearWrap, pin.PosL);
}