#pragma once
#include <render/pass.h>
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

	class OitppllConstants
	{
	public:
		uint32_t OitppllDepthMapIdx;
		DirectX::XMUINT3 OitppllPad0;
	};

	class OitppllPass : public Pass
	{
	public:
		OitppllPass(GlobalResources* globalResources, DXGI_FORMAT outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM);

		virtual void Draw();
		virtual void Update();
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers();

		static void InitOitppllCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
	protected:
		virtual void CopyDescriptors();
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources();
		virtual void InitDescriptors();

		void DrawPpll();
		void DrawOit();

		enum
		{
			PPLL_UAV, OFFSET_UAV, COUNTER_UAV, OITPPLL_UAV_COUNT
		};

		enum
		{
			PPLL_SRV, OFFSET_SRV, OITPPLL_SRV_COUNT
		};


		std::unique_ptr<DefaultResource> mOitppllBuffer;
		std::unique_ptr<DefaultResource> mStartOffsetBuffer; 
		std::unique_ptr<DefaultResource> mCounterBuffer;

		DXGI_FORMAT mOutputFormat;
		
		std::unique_ptr<OitppllConstants> mOitppllConstants;
		std::unique_ptr<HeapAllocInfo> mOitppllCBAllocInfo;
		static std::unique_ptr<CircularHeap> OitppllCBHeap;

	};
}
