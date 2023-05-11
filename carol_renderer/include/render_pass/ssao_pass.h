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
		SsaoPass(
			uint32_t blurCount = 3,
			DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM);

		virtual void Draw()override;
		virtual void OnResize(uint32_t width, uint32_t height)override;

		void ReleaseIntermediateBuffers();

		uint32_t GetRandVecSrvIdx()const;
		uint32_t GetSsaoSrvIdx()const;
		uint32_t GetSsaoUavIdx()const;

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void InitRandomVectorMap();

		std::unique_ptr<ColorBuffer> mRandomVecMap;
		std::unique_ptr<ColorBuffer> mAmbientMap;

		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;
		uint32_t mBlurCount;

		std::unique_ptr<ComputePSO> mSsaoComputePSO;

		std::unique_ptr<EpfPass> mEpfPass;
	};
}

