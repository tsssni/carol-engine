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
		ToneMappingPass();

		virtual void Draw();
	protected:
		virtual void InitPSOs();
		virtual void InitBuffers();

		std::unique_ptr<ComputePSO> mLDRToneMappingComputePSO;
	};
}
