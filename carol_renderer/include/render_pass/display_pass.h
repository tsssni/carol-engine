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
	class ColorBuffer;
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
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS
		);
		DisplayPass(const DisplayPass&) = delete;
		DisplayPass(DisplayPass&&) = delete;
		DisplayPass& operator=(const DisplayPass&) = delete;
	
		virtual IDXGISwapChain* GetSwapChain();
		IDXGISwapChain** GetAddressOfSwapChain();

		void SetBackBufferIndex();
		uint32_t GetBackBufferCount()const;

		ColorBuffer* GetFrameMap()const;
		ColorBuffer* GetDepthStencilMap()const;
		uint32_t GetFrameMapUavIdx()const;
		uint32_t GetDepthStencilSrvIdx()const;

		DXGI_FORMAT GetFrameFormat()const;
		DXGI_FORMAT GetDepthStencilFormat()const;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;

		void Present();
	private:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
		uint32_t mBackBufferIdx;

		std::vector<std::unique_ptr<Resource>> mBackBuffer;
		std::unique_ptr<ColorBuffer> mFrameMap;
		std::unique_ptr<ColorBuffer> mDepthStencilMap;

		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilFormat;
	};
}


