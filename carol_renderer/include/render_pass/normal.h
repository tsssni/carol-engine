#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;

	class NormalPass : public RenderPass
	{
	public:
		NormalPass(DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM);
		NormalPass(const NormalPass&) = delete;
		NormalPass(NormalPass&&) = delete;
		NormalPass& operator=(const NormalPass&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void ReleaseIntermediateBuffers()override;

		uint32_t GetNormalSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		std::unique_ptr<ColorBuffer> mNormalMap;
		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	};
}

