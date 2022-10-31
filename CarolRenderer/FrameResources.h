#pragma once
#include "DirectX/Buffer.h"
#include "Resource/Light.h"
#include <d3d12.h>
#include <DirectXMath.h>

#define MAX_LIGHTS 16

namespace Carol
{
	class Device;

	class PassConstants
	{
	public:
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 InvView;
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 HistViewProj;
		DirectX::XMFLOAT4X4 InvViewProj;
		DirectX::XMFLOAT4X4 ViewProjTex;
		
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float CbPad1 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float CbPad2;
		float CbPad3;

		Light Lights[MAX_LIGHTS];
	};

	class ObjectConstants
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;
		DirectX::XMFLOAT4X4 TexTransform;
		UINT     MatTBIndex;
		UINT     ObjPad0;
		UINT     ObjPad1;
		UINT     ObjPad2;
	};

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256];
		DirectX::XMFLOAT4X4 HistoryFinalTransforms[256];
	};

	class SsaoConstants {
	public:
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ProjTex;
		DirectX::XMFLOAT4 OffsetVectors[14];

		DirectX::XMFLOAT4 BlurWeights[3];
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f,0.0f };

		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 2.0f;
		float SurfaceEplison = 0.05f;
	};

	class FrameResource
	{
	public:
		FrameResource(ID3D12Device* device, int numPassCB,
			int numObjCB, int numSkinnedCB);
	
		UploadBuffer<PassConstants> PassCB;
		UploadBuffer<ObjectConstants> ObjCB;
		UploadBuffer<SkinnedConstants> SkinnedCB;
		UploadBuffer<SsaoConstants> SsaoCB;

		ComPtr<ID3D12CommandAllocator> FRCommandAllocator;
		uint64_t Fence = 0;
	};
}
