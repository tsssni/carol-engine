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
	class ComputePSO;
	class EpfPass;

	class SsaoPass : public RenderPass
	{
	public:
		SsaoPass(DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM);

		virtual void Draw()override;
		virtual void OnResize(uint32_t width, uint32_t height)override;
		void SetBlurRadius(uint32_t blurRadius);
		void SetBlurCount(uint32_t blurCount);

		uint32_t GetSsaoSrvIdx()const;
		uint32_t GetSsaoUavIdx()const;

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		std::unique_ptr<ColorBuffer> mAmbientMap;

		DXGI_FORMAT mAmbientMapFormat;
		uint32_t mBlurCount;

		std::unique_ptr<ComputePSO> mSsaoComputePSO;

		std::unique_ptr<EpfPass> mEpfPass;
	};
}

