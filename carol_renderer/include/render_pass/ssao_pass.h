#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

namespace Carol
{
	class ColorBuffer;
	class Heap;
	class DescriptorManager;

	class SsaoPass : public RenderPass
	{
	public:
		SsaoPass(
			ID3D12Device* device,	
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager,
			uint32_t blurCount = 3,
			DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM);
		SsaoPass(const SsaoPass&) = delete;
		SsaoPass(SsaoPass&&) = delete;
		SsaoPass& operator=(const SsaoPass&) = delete;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
		virtual void OnResize(uint32_t width, uint32_t height, ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		void ReleaseIntermediateBuffers();
		std::vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])const;

		uint32_t GetRandVecSrvIdx()const;
		uint32_t GetSsaoSrvIdx()const;
		uint32_t GetSsaoUavIdx()const;

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		void InitRandomVectors();
		void InitRandomVectorMap(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);

		std::unique_ptr<ColorBuffer> mRandomVecMap;
		std::unique_ptr<ColorBuffer> mAmbientMap;

		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;
		uint32_t mBlurCount;
		DirectX::XMFLOAT4 mOffsets[14];
	};
}

