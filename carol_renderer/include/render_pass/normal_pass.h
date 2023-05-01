#pragma once
#include <render_pass/render_pass.h>
#include <scene/mesh.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;
	class MeshPSO;

	class NormalPass : public RenderPass
	{
	public:
		NormalPass(ID3D12Device* device, DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT normalDsvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
		
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);
		void SetDepthStencilMap(ColorBuffer* depthStencilMap);
		uint32_t GetNormalSrvIdx()const;

	protected:
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		std::unique_ptr<ColorBuffer> mNormalMap;
		ColorBuffer* mDepthStencilMap;

		DXGI_FORMAT mNormalMapFormat;
		DXGI_FORMAT mFrameDsvFormat;

		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;
		
		std::unique_ptr<MeshPSO> mNormalsStaticMeshPSO;
		std::unique_ptr<MeshPSO> mNormalsSkinnedMeshPSO;
	};
}

