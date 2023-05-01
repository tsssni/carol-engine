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
		void SetFrameMap(ColorBuffer* frameMap);
	protected:
		virtual void InitPSOs();
		virtual void InitBuffers();

		ColorBuffer* mFrameMap;
		std::unique_ptr<ComputePSO> mLDRToneMappingComputePSO;
	};
}
