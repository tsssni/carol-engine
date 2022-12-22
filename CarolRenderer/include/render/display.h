#pragma once
#include <render/pass.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <vector>
#include <string>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class Resource;
	class DefaultResource;
	class CpuDescriptorAllocInfo;
	class GpuDescriptorAllocInfo;

	class Display : public Pass
	{
	public:
		Display(
			GlobalResources* globalResources,
			HWND hwnd,
			IDXGIFactory* factory,
			uint32_t width,
			uint32_t height,
			uint32_t bufferCount,
			DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);
		Display(const Display&) = delete;
		Display(Display&&) = delete;
		Display& operator=(const Display&) = delete;
	
		virtual IDXGISwapChain* GetSwapChain();
		IDXGISwapChain** GetAddressOfSwapChain();

		void SetBackBufferIndex();
		uint32_t GetBackBufferCount();
		Resource* GetCurrBackBuffer();
		Resource* GetDepthStencilBuffer();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferRtv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilDsv();
		DXGI_FORMAT GetBackBufferFormat();
		DXGI_FORMAT GetDepthStencilFormat();
		uint32_t GetDepthStencilSrvIdx();

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		void Present();
	private:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;

		uint32_t mCurrBackBufferIndex;
		std::vector<std::unique_ptr<Resource>> mBackBuffer;

		std::unique_ptr<DefaultResource> mDepthStencilBuffer;

		enum
		{
			DEPTH_STENCIL_TEX2D_SRV, DISPLAY_TEX2D_SRV_COUNT
		};

		enum
		{
			DEPTH_STENCIL_DSV, DISPLAY_DSV_COUNT
		};

		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	};

}


