#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <memory>

#define MAX_LIGHTS 16

namespace Carol
{
	class ColorBuffer;
	class StructuredBuffer;

	class OitppllNode
	{
	public:
		DirectX::XMFLOAT4 Color;
		uint32_t Depth;
		uint32_t Next;
	};

	class FramePass :public RenderPass
	{
	public:
		FramePass(
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		FramePass(const FramePass&) = delete;
		FramePass(FramePass&&) = delete;
		FramePass& operator=(const FramePass&) = delete;
		
		virtual void Draw()override;
		void Cull();
		virtual void Update()override;
		virtual void ReleaseIntermediateBuffers()override;

		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameRtv();
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameDsv();
		DXGI_FORMAT GetFrameRtvFormat();
		DXGI_FORMAT GetFrameDsvFormat();
		StructuredBuffer* GetIndirectCommandBuffer(MeshType type);

		uint32_t GetFrameSrvIdx();
		uint32_t GetDepthStencilSrvIdx();
		uint32_t GetHiZSrvIdx();
		uint32_t GetHiZUavIdx();

		uint32_t GetPpllUavIdx();
		uint32_t GetOffsetUavIdx();
		uint32_t GetCounterUavIdx();
		uint32_t GetPpllSrvIdx();
		uint32_t GetOffsetSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		void TestCommandBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements);
		void ResizeCommandBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize);
		
		void DrawOpaque();
		void DrawTransparent();

		void Clear();
        void CullInstances(bool hist, bool opaque);
		void CullMeshlets(bool hist, bool opaque);
		void GenerateHiZ();

		enum
        {
            HIZ_DEPTH_IDX,
            HIZ_R_IDX,
            HIZ_W_IDX,
            HIZ_SRC_MIP,
            HIZ_NUM_MIP_LEVEL,
            HIZ_IDX_COUNT
        };

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

		std::unique_ptr<ColorBuffer> mFrameMap;
		std::unique_ptr<ColorBuffer> mDepthStencilMap;
		std::unique_ptr<ColorBuffer> mHiZMap;

		std::unique_ptr<StructuredBuffer> mOitppllBuffer;
		std::unique_ptr<RawBuffer> mStartOffsetBuffer; 
		std::unique_ptr<RawBuffer> mCounterBuffer;

        std::vector<std::vector<uint32_t>> mCullIdx;
		std::vector<uint32_t> mHiZIdx;
		uint32_t mHiZMipLevels;
		
		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilFormat;
		DXGI_FORMAT mHiZFormat;
	};
}