#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;
	
	class TaaPass : public RenderPass
	{
	public:
		TaaPass(
			DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
		TaaPass(const TaaPass&) = delete;
		TaaPass(TaaPass&&) = delete;
		TaaPass& operator=(const TaaPass&) = delete;

		virtual void Draw()override;
		virtual void Update()override;

		void GetHalton(float& proj0,float& proj1);
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj();
		
		uint32_t GetVeloctiySrvIdx();
		uint32_t GetHistFrameSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void InitHalton();
		float RadicalInversion(int base, int num);

        void DrawVelocityMap();
        void DrawOutput();

		std::unique_ptr<ColorBuffer> mHistFrameMap;
		std::unique_ptr<ColorBuffer> mVelocityMap;

		DXGI_FORMAT mVelocityMapFormat = DXGI_FORMAT_R16G16_SNORM;
		DXGI_FORMAT mFrameFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
	};
}


