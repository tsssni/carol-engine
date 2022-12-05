#pragma once
#include "../Manager.h"
#include "../../DirectX/Resource.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <vector>
#include <string>
#include <memory>
using Microsoft::WRL::ComPtr;
using std::vector;
using std::unique_ptr;
using std::wstring;
using std::to_wstring;

namespace Carol
{
	class CpuDescriptorAllocInfo;
	class GpuDescriptorAllocInfo;

	class DisplayManager : public Manager
	{
	public:
		DisplayManager(
			RenderData* renderData,
			HWND hwnd,
			IDXGIFactory* factory,
			uint32_t width,
			uint32_t height,
			uint32_t bufferCount,
			DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);
		DisplayManager(const DisplayManager&) = delete;
		DisplayManager(DisplayManager&&) = delete;
		DisplayManager& operator=(const DisplayManager&) = delete;
	
		virtual IDXGISwapChain* GetSwapChain();
		IDXGISwapChain** GetAddressOfSwapChain();

		void SetBackBufferIndex();
		uint32_t GetBackBufferCount();
		Resource* GetCurrBackBuffer();
		Resource* GetDepthStencilBuffer();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferView();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView();
		DXGI_FORMAT GetBackBufferFormat();
		DXGI_FORMAT GetDepthStencilFormat();

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetDepthStencilSrv();

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		void Present();
	private:
		virtual void InitRootSignature()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		ComPtr<IDXGISwapChain> mSwapChain;

		uint32_t mCurrBackBufferIndex;
		vector<unique_ptr<Resource>> mBackBuffer;

		unique_ptr<DefaultResource> mDepthStencilBuffer;
		enum
		{
			DEPTH_STENCIL_SRV, DISPLAY_SRV_COUNT
		};

		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	};

}


