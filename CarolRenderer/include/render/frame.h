#pragma once
#include <scene/light.h>
#include <DirectXMath.h>

#define MAX_LIGHTS 16

namespace Carol {
	class FrameConstants
	{
	public:
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 InvView;
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 InvViewProj;
		DirectX::XMFLOAT4X4 ProjTex;
		DirectX::XMFLOAT4X4 ViewProjTex;

		DirectX::XMFLOAT4X4 HistViewProj;
		DirectX::XMFLOAT4X4 JitteredViewProj;

		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float CbPad0 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		uint32_t ShadowMapIdx = 0;
		uint32_t SsaoMapIdx = 0;

		Light Lights[MAX_LIGHTS];
	};
}


