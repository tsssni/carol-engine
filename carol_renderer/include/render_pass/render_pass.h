#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>
#include <string_view>

namespace Carol
{
	class StructuredBuffer;
	class Heap;
	class DescriptorManager;
	class RootSignature;

	class RenderPass
	{
	public:
		virtual void Draw(ID3D12GraphicsCommandList* cmdList) = 0;
		virtual void OnResize(
			uint32_t width,
			uint32_t height,
			ID3D12Device* device,
			Heap* heap,
			DescriptorManager* descriptorManager);

		static void Init(ID3D12Device* device);
		static const RootSignature* GetRootSignature();

	protected:
		virtual void InitPSOs(ID3D12Device* device) = 0;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager) = 0;
		
		void ExecuteIndirect(ID3D12GraphicsCommandList* cmdList, const StructuredBuffer* indirectCmdBuffer);

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
		uint32_t mWidth;
		uint32_t mHeight;
		uint32_t mMipLevel;

		static std::unique_ptr<RootSignature> sRootSignature;
		static Microsoft::WRL::ComPtr<ID3D12CommandSignature> sCommandSignature;
	};
}
