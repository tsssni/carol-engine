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

		void SetBlurCount(uint32_t blurCout);
		std::vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		uint32_t GetRandVecSrvIdx();
		uint32_t GetSsaoSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		void InitRandomVectors();
		void InitRandomVectorMap();

		void DrawSsao();
		void DrawAmbientMap();
		void DrawAmbientMap(bool vertBlur);

		std::unique_ptr<DefaultResource> mRandomVecMap;
		std::unique_ptr<DefaultResource> mAmbientMap0;
		std::unique_ptr<DefaultResource> mAmbientMap1;

		uint32_t mBlurCount = 3;
		
		enum
		{
			RAND_VEC_SRV, AMBIENT0_SRV, AMBIENT1_SRV, SSAO_CBV_SRV_UAV_COUNT
		};

		enum
		{
			AMBIENT0_RTV, AMBIENT1_RTV, SSAO_RTV_COUNT
		};

		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;
		DirectX::XMFLOAT4 mOffsets[14];
	};
}

