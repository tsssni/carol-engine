#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DescriptorAllocInfo;

	class RenderPass
	{
	public:
		RenderPass(GlobalResources* globalResources);
		RenderPass(const RenderPass&) = delete;
		RenderPass(RenderPass&&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;

		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize() = 0;
		virtual void ReleaseIntermediateBuffers() = 0;

	protected:
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitBuffers() = 0;

		GlobalResources* mGlobalResources;
	};
}
