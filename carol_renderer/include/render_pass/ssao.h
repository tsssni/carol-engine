#pragma once
#include <render_pass/render_pass.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

namespace Carol
{
	class ColorBuffer;

	class SsaoPass : public RenderPass
	{
	public:
		SsaoPass(
			uint32_t blurCount = 3,
			DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM);
		SsaoPass(const SsaoPass&) = delete;
		SsaoPass(SsaoPass&&) = delete;
		SsaoPass& operator=(const SsaoPass&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void ReleaseIntermediateBuffers()override;

		void SetBlurCount(uint32_t blurCout);
		std::vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		uint32_t GetRandVecSrvIdx();
		uint32_t GetSsaoSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void InitRandomVectors();
		void InitRandomVectorMap();

		void DrawSsao();
		void DrawAmbientMap();
		void DrawAmbientMap(bool vertBlur);

		std::unique_ptr<ColorBuffer> mRandomVecMap;
		std::unique_ptr<ColorBuffer> mAmbientMap0;
		std::unique_ptr<ColorBuffer> mAmbientMap1;

		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;
		uint32_t mBlurCount;
		DirectX::XMFLOAT4 mOffsets[14];
	};
}

