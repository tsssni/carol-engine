#pragma once
#include <render/pass.h>
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
	
	class TaaConstants
	{
	public:
		uint32_t TaaDepthMapIdx = 0;
		uint32_t TaaHistFrameMapIdx = 0;
		uint32_t TaaCurrFrameMapIdx = 0;
		uint32_t TaaVelocityMapIdx = 0;
	};

	class TaaPass : public Pass
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

		static void InitTaaCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

		void GetHalton(float& proj0,float& proj1);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrFrameRtv();
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj();

	protected:
		virtual void CopyDescriptors()override;
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
			HIST_TEX2D_SRV, CURR_TEX2D_SRV, VELOCITY_TEX2D_SRV, TAA_TEX2D_SRV_COUNT
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

		std::unique_ptr<TaaConstants> mTaaConstants;
		std::unique_ptr<HeapAllocInfo> mTaaCBAllocInfo;
		static std::unique_ptr<CircularHeap> TaaCBHeap;
	};
}


