#pragma once
#include <render_pass/render_pass.h>
#include <memory>
#include <vector>

namespace Carol
{
	class ColorBuffer;
	class ComputePSO;

	class ToneMappingPass : public RenderPass
	{
	public:
		ToneMappingPass(ID3D12Device* device);

		virtual void Draw(ID3D12GraphicsCommandList* cmdList);
		void SetFrameMap(ColorBuffer* frameMap);
	protected:
		virtual void InitPSOs(ID3D12Device* device);
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager);

		ColorBuffer* mFrameMap;
		std::unique_ptr<ColorBuffer> mAdaptedLuminanceMap;

		std::vector<uint32_t> mAdaptedLuminanceIdx;

		std::unique_ptr<ComputePSO> mLDRToneMappingComputePSO;
	};
}
