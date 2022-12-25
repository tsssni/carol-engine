#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

namespace Carol
{
	class GlobalResources;
	class HeapAllocInfo;
	class CircularHeap;
	class DefaultResource;
	class Shader;

	class SsaoConstants {
	public:
		DirectX::XMFLOAT4 OffsetVectors[14];
		DirectX::XMFLOAT4 BlurWeights[3];

		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 1.0f;
		float SurfaceEplison = 0.05f;
	};

	class SsaoPass : public RenderPass
	{
	public:
		SsaoPass(
			GlobalResources* globalResources,
			uint32_t blurCount = 3,
			DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM);
		SsaoPass(const SsaoPass&) = delete;
		SsaoPass(SsaoPass&&) = delete;
		SsaoPass& operator=(const SsaoPass&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		static void InitSsaoCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

		void SetBlurCount(uint32_t blurCout);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetSsaoSrv();
	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		void InitRandomVectors();
		void InitRandomVectorMap();
		void InitConstants();

		void DrawSsao();
		void DrawAmbientMap();
		void DrawAmbientMap(bool vertBlur);

		std::vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		std::unique_ptr<DefaultResource> mRandomVecMap;
		std::unique_ptr<DefaultResource> mAmbientMap0;
		std::unique_ptr<DefaultResource> mAmbientMap1;

		uint32_t mBlurCount = 3;
		
		enum
		{
			RAND_VEC_SRV, AMBIENT0_SRV, AMBIENT1_SRV, SSAO_SRV_COUNT
		};

		enum
		{
			AMBIENT0_RTV, AMBIENT1_RTV, SSAO_RTV_COUNT
		};

		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;

		DirectX::XMFLOAT4 mOffsets[14];

		std::unique_ptr<SsaoConstants> mSsaoConstants;
		std::unique_ptr<HeapAllocInfo> mSsaoCBAllocInfo;
		static std::unique_ptr<CircularHeap> SsaoCBHeap;
	};
}

