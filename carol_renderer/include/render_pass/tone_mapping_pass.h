#pragma once
#include <render_pass/render_pass.h>
#include <memory>
#include <vector>

namespace Carol
{
	class ColorBuffer;

	class ToneMappingPass : public RenderPass
	{
	public:
		ToneMappingPass(ID3D12Device* device, DXGI_FORMAT adaptedLuminanceFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw(ID3D12GraphicsCommandList* cmdList);
		void SetFrameMap(ColorBuffer* frameMap);
		uint32_t GetAdaptedLuminanceMapUavIdx();
		uint32_t GetAdaptedLuminanceMapSrvIdx();
	protected:
		virtual void InitShaders();
		virtual void InitPSOs(ID3D12Device* device);
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager);

		void DrawAdaptedLuminance(ID3D12GraphicsCommandList* cmdList);
		void DrawToneMapping(ID3D12GraphicsCommandList* cmdList);

		enum
		{
			ADAPTED_LUMINANCE_SRC_MIP,
			ADAPTED_LUMINANCE_NUM_MIP_LEVEL,
			ADAPTED_LUMINANCE_IDX_COUNT
		};

		ColorBuffer* mFrameMap;
		std::unique_ptr<ColorBuffer> mAdaptedLuminanceMap;

		DXGI_FORMAT mAdaptedLuminanceFormat;

		std::vector<uint32_t> mAdaptedLuminanceIdx;
	};
}
