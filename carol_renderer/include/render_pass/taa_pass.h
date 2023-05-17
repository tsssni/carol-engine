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
		TaaPass(DXGI_FORMAT frameMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT);

		virtual void Draw()override;

		void GetHalton(float& proj0,float& proj1)const;

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void InitHalton();
		float RadicalInversion(int base, int num);


		std::unique_ptr<ColorBuffer> mHistMap;
		std::unique_ptr<ColorBuffer> mVelocityMap;

		DXGI_FORMAT mFrameMapFormat;

		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
		
		std::unique_ptr<ComputePSO> mTaaComputePSO;
	};
}


