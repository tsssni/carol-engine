#pragma once
#include <render_pass/render_pass.h>
#include <scene/mesh.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;

	class NormalPass : public RenderPass
	{
	public:
		NormalPass(ID3D12Device* device, DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT normalDsvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		NormalPass(const NormalPass&) = delete;
		NormalPass(NormalPass&&) = delete;
		NormalPass& operator=(const NormalPass&) = delete;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
		
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);
		uint32_t GetNormalSrvIdx()const;

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		std::unique_ptr<ColorBuffer> mNormalMap;
		std::unique_ptr<ColorBuffer> mNormalDepthStencilMap;
		DXGI_FORMAT mNormalMapFormat;
		DXGI_FORMAT mNormalDsvFormat;

		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;
	};
}

