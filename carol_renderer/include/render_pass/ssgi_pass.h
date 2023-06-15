#pragma once
#include <render_pass/render_pass.h>
#include <d3d12.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;
	class ComputePSO;
	class FastConstantBufferAllocator;

	class SceneColorConstants
	{
	public:
		uint32_t SrcMipLevel;
		uint32_t NumMipLevel;
	};

	class SsgiPass : public RenderPass
	{
	public:
		SsgiPass(DXGI_FORMAT ssgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT ssgiHiZFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw()override;

		uint32_t GetSceneColorSrvIdx();
		uint32_t GetSceneColorUavIdx();
		uint32_t GetSsgiHiZSrvIdx();
		uint32_t GetSsgiHiZUavIdx();
		uint32_t GetSsgiSrvIdx();
		uint32_t GetSsgiUavIdx();

		void SetSampleCount(uint32_t sampleCount);
		void SetNumSteps(uint32_t numSteps);

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void Generate();
		void DrawSsgi();

		std::unique_ptr<SceneColorConstants> mSceneColorConstants;
		D3D12_GPU_VIRTUAL_ADDRESS mSceneColorCBAddr;
		std::unique_ptr<FastConstantBufferAllocator> mSceneColorCBAllocator;

		std::unique_ptr<ColorBuffer> mSceneColorMap;
		std::unique_ptr<ColorBuffer> mSsgiHiZMap;
		std::unique_ptr<ColorBuffer> mSsgiMap;

		DXGI_FORMAT mSsgiFormat;
		DXGI_FORMAT mSsgiHiZFormat;

		uint32_t mSampleCount;
		uint32_t mNumSteps;

		std::unique_ptr<ComputePSO> mSsgiGenerateComputePSO;
		std::unique_ptr<ComputePSO> mSsgiComputePSO;
	};
}
