#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <DirectXMath.h>
#include <memory>

#define MAX_LIGHTS 16

namespace Carol
{
	class GlobalResources;
	class DefaultResource;
	class CircularHeap;
	class HeapAllocInfo;

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
		float FrameCBPad0 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		DirectX::XMFLOAT2 FrameCBPad1;

		Light Lights[MAX_LIGHTS];
	};

	class FramePass :public RenderPass
	{
	public:
		FramePass(GlobalResources* globalResources, DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT depthStencilResourceFormat = DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT depthStencilDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT depthStencilSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		FramePass(const FramePass&) = delete;
		FramePass(FramePass&&) = delete;
		FramePass& operator=(const FramePass&) = delete;
		
		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		static void InitFrameCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

		D3D12_GPU_VIRTUAL_ADDRESS GetFrameAddress();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetFrameSrv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetFrameRtv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilDsv();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetDepthStencilSrv();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		enum
		{
			FRAME_SRV, DEPTH_STENCIL_SRV, FRAME_SRV_COUNT
		};

		enum
		{
			FRAME_RTV, FRAME_RTV_COUNT
		};

		enum
		{
			DEPTH_STENCIL_DSV, FRAME_DSV_COUNT
		};

		std::unique_ptr<DefaultResource> mFrameMap;
		std::unique_ptr<DefaultResource> mDepthStencilMap;

		std::unique_ptr<FrameConstants> mFrameConstants;
        std::unique_ptr<HeapAllocInfo> mFrameCBAllocInfo;
        static std::unique_ptr<CircularHeap> FrameCBHeap;

		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilResourceFormat;
		DXGI_FORMAT mDepthStencilDsvFormat;
		DXGI_FORMAT mDepthStencilSrvFormat;
	};
}