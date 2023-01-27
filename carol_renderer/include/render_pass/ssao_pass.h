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
		virtual void OnResize(uint32_t width, uint32_t height)override;

		std::vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		uint32_t GetRandVecSrvIdx();
		uint32_t GetSsaoSrvIdx();
		uint32_t GetSsaoUavIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void InitRandomVectors();
		void InitRandomVectorMap();

		std::unique_ptr<ColorBuffer> mRandomVecMap;
		std::unique_ptr<ColorBuffer> mAmbientMap;

		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;
		uint32_t mBlurCount;
		DirectX::XMFLOAT4 mOffsets[14];
	};
}

