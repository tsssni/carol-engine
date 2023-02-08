#pragma once
#include <render_pass/render_pass.h>
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
	class Resource;
	class DescriptorAllocInfo;
	class DescriptorManager;

	class DisplayPass : public RenderPass
	{
	public:
		DisplayPass(
			HWND hwnd,
			IDXGIFactory* factory,
			ID3D12CommandQueue* cmdQueue,
			uint32_t bufferCount,
			DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM
		);
		DisplayPass(const DisplayPass&) = delete;
		DisplayPass(DisplayPass&&) = delete;
		DisplayPass& operator=(const DisplayPass&) = delete;
		~DisplayPass();
	
		virtual IDXGISwapChain* GetSwapChain();
		IDXGISwapChain** GetAddressOfSwapChain();

		void SetBackBufferIndex();
		uint32_t GetBackBufferCount();
		Resource* GetCurrBackBuffer();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferRtv();
		DXGI_FORMAT GetBackBufferFormat();

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;

		void Present();
	private:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;
		void InitDescriptors(DescriptorManager* descriptorManager);

		Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
		uint32_t mCurrBackBufferIndex;

		std::vector<std::unique_ptr<Resource>> mBackBuffer;
		std::unique_ptr<DescriptorAllocInfo> mBackBufferRtvAllocInfo;
		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	};
}


