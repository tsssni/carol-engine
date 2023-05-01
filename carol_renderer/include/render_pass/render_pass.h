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
		virtual void Draw() = 0;
		virtual void OnResize(
			uint32_t width,
			uint32_t height);

		static void Init();
		static const RootSignature* GetRootSignature();

	protected:
		virtual void InitPSOs() = 0;
		virtual void InitBuffers() = 0;
		
		void ExecuteIndirect(const StructuredBuffer* indirectCmdBuffer);

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
		uint32_t mWidth;
		uint32_t mHeight;
		uint32_t mMipLevel;

		static std::unique_ptr<RootSignature> sRootSignature;
		static Microsoft::WRL::ComPtr<ID3D12CommandSignature> sCommandSignature;
	};
}
