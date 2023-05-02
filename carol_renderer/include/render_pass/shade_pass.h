#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

#define MAX_LIGHTS 16

namespace Carol
{
	class ColorBuffer;
	class StructuredBuffer;
	class StructuredBufferPool;
	class MeshPSO;

	class ShadePass :public RenderPass
	{
	public:
		ShadePass(
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw();

		void SetFrameMap(ColorBuffer* frameMap);
		void SetDepthStencilMap(ColorBuffer* depthStencilMap);

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers();
		
		void DrawSkyBox(D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr);

		ColorBuffer* mFrameMap;
		ColorBuffer* mDepthStencilMap;

		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilFormat;

		std::unique_ptr<ComputePSO> mShadeComputePSO;
		std::unique_ptr<MeshPSO> mSkyBoxMeshPSO;
	};
}