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
		NormalPass(ID3D12Device* device, DXGI_FORMAT frameDsvFormat, DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM);
		NormalPass(const NormalPass&) = delete;
		NormalPass(NormalPass&&) = delete;
		NormalPass& operator=(const NormalPass&) = delete;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
		
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);
		void SetFrameDsv(D3D12_CPU_DESCRIPTOR_HANDLE frameDsv);
		uint32_t GetNormalSrvIdx()const;

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		std::unique_ptr<ColorBuffer> mNormalMap;
		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;
		D3D12_CPU_DESCRIPTOR_HANDLE mFrameDsv;
		DXGI_FORMAT mFrameDsvFormat;
	};
}

