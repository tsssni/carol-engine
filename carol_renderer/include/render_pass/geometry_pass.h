#pragma once
#include <render_pass/render_pass.h>
#include <d3d12.h>

namespace Carol
{
	class ColorBuffer;
	class MeshPSO;
	class CullPass;

	class GeometryPass : public RenderPass
	{
	public:
		GeometryPass(
			DXGI_FORMAT diffuseRoughnessFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT emissiveMetallicFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT normalDepthFormat = DXGI_FORMAT_R32G32B32A32_FLOAT,
			DXGI_FORMAT velocityFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

		void SetDepthStencilMap(ColorBuffer* depthStencilMap);
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);

		uint32_t GetDiffuseRoughnessMapSrvIdx();
		uint32_t GetEmissiveMetallicMapSrvIdx();
		uint32_t GetNormalDepthMapSrvIdx();
		uint32_t GetVelocityMapSrvIdx();

		virtual void Draw()override;
	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		std::unique_ptr<ColorBuffer> mDiffuseRoughnessMap;
		std::unique_ptr<ColorBuffer> mEmissiveMetallicMap;
		std::unique_ptr<ColorBuffer> mNormalDepthMap;
		std::unique_ptr<ColorBuffer> mVelocityMap;
		ColorBuffer* mDepthStencilMap;
		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;

		DXGI_FORMAT mDiffuseRoughnessFormat;
		DXGI_FORMAT mEmissiveMetallicFormat;
		DXGI_FORMAT mNormalDepthFormat;
		DXGI_FORMAT mVelocityFormat;
		DXGI_FORMAT mDepthStencilFormat;

		std::unique_ptr<MeshPSO> mGeometryStaticMeshPSO;
		std::unique_ptr<MeshPSO> mGeometrySkinnedMeshPSO;
	};
}
