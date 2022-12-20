#pragma once
#include <manager/manager.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DefaultResource;
	class Shader;

	class TaaManager : public Manager
	{
	public:
		TaaManager(
			GlobalResources* globalResources,
			DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
		TaaManager(const TaaManager&) = delete;
		TaaManager(TaaManager&&) = delete;
		TaaManager& operator=(const TaaManager&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		void GetHalton(float& proj0,float& proj1);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrFrameRtv();
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj();

	protected:
		virtual void InitRootSignature()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		void InitHalton();
		float RadicalInversion(int base, int num);

		void DrawCurrFrameMap();
        void DrawVelocityMap();
        void DrawOutput();

		enum
		{
			HIST_SRV, CURR_SRV, VELOCITY_SRV, TAA_SRV_COUNT
		};

		enum
		{
			CURR_RTV, VELOCITY_RTV, TAA_RTV_COUNT
		};

		std::unique_ptr<DefaultResource> mHistFrameMap;
		std::unique_ptr<DefaultResource> mCurrFrameMap;
		std::unique_ptr<DefaultResource> mVelocityMap;

		DXGI_FORMAT mVelocityMapFormat = DXGI_FORMAT_R16G16_SNORM;
		DXGI_FORMAT mFrameFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
	};
}


