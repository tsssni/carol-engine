#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class RenderPass
	{
	public:
		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize(uint32_t width, uint32_t height);
		virtual void ReleaseIntermediateBuffers() = 0;

	protected:
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitBuffers() = 0;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
		uint32_t mWidth;
		uint32_t mHeight;
	};
}
