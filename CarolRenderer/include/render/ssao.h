#pragma once
#include <render/pass.h>
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

		uint32_t SsaoDepthMapIdx = 0;
		uint32_t SsaoNormalMapIdx = 0;
		uint32_t SsaoRandVecMapIdx = 0;
		uint32_t SsaoAmbientMapIdx = 0;
		uint32_t BlurDirection = 0;
		DirectX::XMFLOAT3 SsaoPad0;
	};

	class SsaoPass : public Pass
	{
	public:
		SsaoPass(
			GlobalResources* globalResources,
			uint32_t blurCount = 6,
			DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R16G16B16A16_SNORM,
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
		uint32_t GetSsaoSrvIdx();
	protected:
		virtual void CopyDescriptors()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		void InitRandomVectors();
		void InitRandomVectorMap();

		void InitConstants();

		void DrawNormalsAndDepth();
		void DrawSsao();
		void DrawAmbientMap();
		void DrawAmbientMap(bool horzBlur);

		std::vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		std::unique_ptr<DefaultResource> mNormalMap;
		std::unique_ptr<DefaultResource> mRandomVecMap;
		std::unique_ptr<DefaultResource> mAmbientMap0;
		std::unique_ptr<DefaultResource> mAmbientMap1;

		uint32_t mBlurCount = 3;

		enum
		{
			NORMAL_TEX2D_SRV, RAND_VEC_TEX2D_SRV, AMBIENT0_TEX2D_SRV, AMBIENT1_TEX2D_SRV, SSAO_TEX2D_SRV_COUNT
		};

		enum
		{
			NORMAL_RTV, AMBIENT0_RTV, AMBIENT1_RTV, SSAO_RTV_COUNT
		};

		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;

		DirectX::XMFLOAT4 mOffsets[14];

		std::unique_ptr<SsaoConstants> mSsaoConstants;
		std::unique_ptr<HeapAllocInfo> mSsaoCBAllocInfo;
		static std::unique_ptr<CircularHeap> SsaoCBHeap;
	};
}

