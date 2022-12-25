#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DefaultResource;

	class NormalPass : public RenderPass
	{
	public:
		NormalPass(GlobalResources* globalResources, DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM);
		NormalPass(const NormalPass&) = delete;
		NormalPass(NormalPass&&) = delete;
		NormalPass& operator=(const NormalPass&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetNormalSrv();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		enum
		{
			NORMAL_SRV, NORMAL_SRV_COUNT
		};

		enum
		{
			NORMAL_RTV, NORMAL_RTV_COUNT
		};

		std::unique_ptr<DefaultResource> mNormalMap;
		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	};
}

