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
		
		virtual void Draw()override;
	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		ColorBuffer* mColorMap;
		std::unique_ptr<ComputePSO> mEpfComputePSO;
	};
}
