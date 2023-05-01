#pragma once
#include <render_pass/render_pass.h>
#include <scene/mesh.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;
	class Resource;
	class MeshPSO;
	class ComputePSO;
	
	class TaaPass : public RenderPass
	{
	public:
		TaaPass(
			DXGI_FORMAT frameMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT velocityDsvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

		virtual void Draw()override;
        void DrawVelocityMap();
        void DrawOutput();
		
		void SetFrameMap(ColorBuffer* frameMap);
		void SetDepthStencilMap(ColorBuffer* depthStencilMap);
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);

		void GetHalton(float& proj0,float& proj1)const;
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj()const;
		
		uint32_t GetVeloctiySrvIdx()const;
		uint32_t GetHistFrameUavIdx()const;

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void InitHalton();
		float RadicalInversion(int base, int num);


		std::unique_ptr<ColorBuffer> mHistMap;
		std::unique_ptr<ColorBuffer> mVelocityMap;
		ColorBuffer* mFrameMap;
		ColorBuffer* mDepthStencilMap;

		DXGI_FORMAT mVelocityMapFormat;
		DXGI_FORMAT mVelocityDsvFormat;
		DXGI_FORMAT mFrameMapFormat;


		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
		
		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;

		std::unique_ptr<MeshPSO> mVelocityStaticMeshPSO;
		std::unique_ptr<MeshPSO> mVelocitySkinnedMeshPSO;
		std::unique_ptr<ComputePSO> mTaaComputePSO;
	};
}


