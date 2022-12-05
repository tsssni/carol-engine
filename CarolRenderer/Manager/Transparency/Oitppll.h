#pragma once
#include "../Manager.h"
#include <d3d12.h>
#include <DirectXPackedVector.h>
#include <memory>

using std::unique_ptr;

namespace Carol
{
	class Buffer;

	class OitppllNode
	{
	public:
		DirectX::PackedVector::XMCOLOR Color{ 1.0f, 1.0f, 1.0f, 1.0f};
		uint32_t Depth = UINT32_MAX;
		uint32_t Next = UINT32_MAX;
	};

	class OitppllManager : public Manager
	{
	public:
		OitppllManager(RenderData* renderData, DXGI_FORMAT outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM);

		virtual void Draw();
		virtual void Update();
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers();
	protected:
		virtual void InitRootSignature()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources();
		virtual void InitDescriptors();

		void DrawPpll();
		void DrawOit();

		enum
		{
			PPLL_UAV, OFFSET_UAV, COUNTER_UAV, PPLL_SRV, OFFSET_SRV, OITPPLL_UAV_SRV_COUNT
		};


		unique_ptr<DefaultResource> mOitppllBuffer;
		unique_ptr<DefaultResource> mStartOffsetBuffer; 
		unique_ptr<DefaultResource> mCounterBuffer;

		DXGI_FORMAT mOutputFormat;

	};
}
