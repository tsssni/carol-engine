#pragma once
#include <render_pass/render_pass.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;

	class UtilsPass : public RenderPass
	{
	public:
		UtilsPass();
		
		virtual void Draw()override;

		uint32_t GetRandVecSrvIdx()const;
	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		std::unique_ptr<ColorBuffer> mRandomVecMap;
	};
}
