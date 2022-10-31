#pragma once
#include "Buffer.h"
#include "DescriptorHeap.h"
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
	class RtvDescriptorHeap;
	class DsvDescriptorHeap;

	class DisplayManager
	{
	public:
		DisplayManager() = default;
		DisplayManager(const DisplayManager&) = delete;
		DisplayManager(DisplayManager&&) = delete;
		DisplayManager& operator=(const DisplayManager&) = delete;
	public:
		virtual IDXGISwapChain* GetSwapChain();
		IDXGISwapChain** GetAddressOfSwapChain();

		void SetBackBufferIndex();
		uint32_t GetBackBufferCount();
		Resource* GetCurrBackBuffer();
		Resource* GetDepthStencilBuffer();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferView(RtvDescriptorHeap* rtvHeap);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView(DsvDescriptorHeap* dsvHeap);
		DXGI_FORMAT GetBackBufferFormat();
		DXGI_FORMAT GetDepthStencilFormat();

		void InitDisplayManager(
			HWND hwnd,
			IDXGIFactory* factory,
			ID3D12CommandQueue* cmdQueue,
			uint32_t width,
			uint32_t height,
			uint32_t bufferCount,
			DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);

		void OnResize(
			ID3D12GraphicsCommandList* cmdList,
			RtvDescriptorHeap* rtvHeap,
			DsvDescriptorHeap* dsvHeap,
			uint32_t width,
			uint32_t height);

		void ClearAndSetRenderTargetAndDepthStencil(
			ID3D12GraphicsCommandList* cmdList,
			RtvDescriptorHeap* rtvHeap,
			DsvDescriptorHeap* dsvHeap,
			DirectX::XMVECTORF32 initColor = DirectX::Colors::Gray,
			float initDepth = 1.0f,
			uint32_t initStencil = 0);

		void Present();
	private:
		ComPtr<IDXGISwapChain> mSwapChain;

		uint32_t mCurrBackBufferIndex;
		vector<unique_ptr<Resource>> mBackBuffer;
		unique_ptr<DefaultBuffer> mDepthStencilBuffer;

		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	};

}


