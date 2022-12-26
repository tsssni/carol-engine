#pragma once
#include <render_pass/render_pass.h>
#include <d3d12.h>
#include <DirectXPackedVector.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DefaultResource;
	class Buffer;
	class HeapAllocInfo;
	class CircularHeap;

	class OitppllNode
	{
	public:
		DirectX::PackedVector::XMCOLOR Color{ 1.0f, 1.0f, 1.0f, 1.0f};
		uint32_t Depth = UINT32_MAX;
		uint32_t Next = UINT32_MAX;
	};

	class OitppllPass : public RenderPass
	{
	public:
		OitppllPass(GlobalResources* globalResources, DXGI_FORMAT outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM);

		virtual void Draw();
		virtual void Update();
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers();
		
		uint32_t GetPpllUavIdx();
		uint32_t GetOffsetUavIdx();
		uint32_t GetCounterUavIdx();
		uint32_t GetPpllSrvIdx();
		uint32_t GetOffsetSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources();
		virtual void InitDescriptors();

		void DrawPpll();
		void DrawOit();
		
		enum
		{
			PPLL_SRV, OFFSET_SRV, PPLL_UAV, OFFSET_UAV, COUNTER_UAV, OITPPLL_CBV_SRV_UAV_COUNT
		};

		std::unique_ptr<DefaultResource> mOitppllBuffer;
		std::unique_ptr<DefaultResource> mStartOffsetBuffer; 
		std::unique_ptr<DefaultResource> mCounterBuffer;

		DXGI_FORMAT mOutputFormat;
	};
}
