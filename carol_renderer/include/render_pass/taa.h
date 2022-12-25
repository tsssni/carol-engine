#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DefaultResource;
	class HeapAllocInfo;
	class CircularHeap;
	class Shader;
	
	class TaaPass : public RenderPass
	{
	public:
		TaaPass(
			GlobalResources* globalResources,
			DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
		TaaPass(const TaaPass&) = delete;
		TaaPass(TaaPass&&) = delete;
		TaaPass& operator=(const TaaPass&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		void GetHalton(float& proj0,float& proj1);
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		void InitHalton();
		float RadicalInversion(int base, int num);

        void DrawVelocityMap();
        void DrawOutput();

		enum
		{
			HIST_SRV, VELOCITY_SRV, TAA_SRV_COUNT
		};

		enum
		{
			VELOCITY_RTV, TAA_RTV_COUNT
		};

		std::unique_ptr<DefaultResource> mHistFrameMap;
		std::unique_ptr<DefaultResource> mVelocityMap;

		DXGI_FORMAT mVelocityMapFormat = DXGI_FORMAT_R16G16_SNORM;
		DXGI_FORMAT mFrameFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
	};
}


