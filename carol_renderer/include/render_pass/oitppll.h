#pragma once
#include <render_pass/render_pass.h>
#include <d3d12.h>
#include <DirectXPackedVector.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class StructuredBuffer;
	class RawBuffer;

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

		void Cull();
		
		uint32_t GetPpllUavIdx();
		uint32_t GetOffsetUavIdx();
		uint32_t GetCounterUavIdx();
		uint32_t GetPpllSrvIdx();
		uint32_t GetOffsetSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers();

		void TestCommandBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements);
		void ResizeCommandBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize);
	
		void DrawPpll();
		void DrawOit();

		enum
        {
            CULL_CULLED_COMMAND_BUFFER_IDX,
            CULL_MESH_COUNT,
            CULL_MESH_OFFSET,
            CULL_HIZ_IDX,
            CULL_HIST,
            CULL_IDX_COUNT
        };

		
		std::vector<std::vector<std::unique_ptr<StructuredBuffer>>> mCulledCommandBuffer;
        std::vector<std::vector<uint32_t>> mCullIdx;

		std::unique_ptr<StructuredBuffer> mOitppllBuffer;
		std::unique_ptr<RawBuffer> mStartOffsetBuffer; 
		std::unique_ptr<RawBuffer> mCounterBuffer;

		DXGI_FORMAT mOutputFormat;
	};
}
