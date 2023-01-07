#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <DirectXMath.h>
#include <memory>

#define MAX_LIGHTS 16

namespace Carol
{
	class GlobalResources;
	class ColorBuffer;
	class StructuredBuffer;

	class FramePass :public RenderPass
	{
	public:
		FramePass(
			GlobalResources* globalResources,
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		FramePass(const FramePass&) = delete;
		FramePass(FramePass&&) = delete;
		FramePass& operator=(const FramePass&) = delete;
		
		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameRtv();
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameDsv();
		uint32_t GetFrameSrvIdx();
		uint32_t GetDepthStencilSrvIdx();
		uint32_t GetHiZSrvIdx();
		uint32_t GetHiZUavIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;
		
		void DrawFrame();
		void DrawHiZ();

		std::unique_ptr<ColorBuffer> mFrameMap;
		std::unique_ptr<ColorBuffer> mDepthStencilMap;
		std::unique_ptr<ColorBuffer> mHiZMap;
		std::vector<std::unique_ptr<StructuredBuffer>> mOcclusionCommandBuffer;

		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilFormat;
		DXGI_FORMAT mHiZFormat;
		uint32_t mHiZMipLevels;
	};
}