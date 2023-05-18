#pragma once
#include <render_pass/render_pass.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;
	class ComputePSO;

	class EpfPass : public RenderPass
	{
	public:
		EpfPass();
		void SetColorMap(ColorBuffer* colorMap);
		void SetBlurRadius(uint32_t blurRadius);
		void SetBlurCount(uint32_t blurCount);
		
		virtual void Draw()override;
	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		ColorBuffer* mColorMap;
		uint32_t mBlurRadius;
		uint32_t mBlurCount;

		std::unique_ptr<ComputePSO> mEpfComputePSO;
	};
}
