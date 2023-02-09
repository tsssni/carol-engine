#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

#define MAX_LIGHTS 16

namespace Carol
{
	class ColorBuffer;
	class StructuredBuffer;
	class StructuredBufferPool;
	class Heap;
	class Scene;
	class DescriptorManager;

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
			ID3D12Device* device,
			Heap* heap,
			DescriptorManager* descriptorManager,
			Scene* scene,
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		FramePass(const FramePass&) = delete;
		FramePass(FramePass&&) = delete;
		FramePass& operator=(const FramePass&) = delete;
		
		virtual void Draw(ID3D12GraphicsCommandList* cmdList);
		void Cull(ID3D12GraphicsCommandList* cmdList);
		void Update(uint64_t cpuFenceValue, uint64_t completedFenceValue);

		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameRtv()const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameDsv()const;
		DXGI_FORMAT GetFrameFormat()const;
		DXGI_FORMAT GetFrameDsvFormat()const;
		const StructuredBuffer* GetIndirectCommandBuffer(MeshType type)const;

		uint32_t GetFrameSrvIdx()const;
		uint32_t GetDepthStencilSrvIdx()const;
		uint32_t GetHiZSrvIdx()const;
		uint32_t GetHiZUavIdx()const;

		uint32_t GetPpllUavIdx()const;
		uint32_t GetOffsetUavIdx()const;
		uint32_t GetCounterUavIdx()const;
		uint32_t GetPpllSrvIdx()const;
		uint32_t GetOffsetSrvIdx()const;

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager);
		
		void DrawOpaque(ID3D12GraphicsCommandList* cmdList);
		void DrawTransparent(ID3D12GraphicsCommandList* cmdList);
		void DrawSkyBox(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr);

		void Clear(ID3D12GraphicsCommandList* cmdList);
        void CullInstances(ID3D12GraphicsCommandList* cmdList, bool hist, bool opaque);
		void CullMeshlets(ID3D12GraphicsCommandList* cmdList, bool hist, bool opaque);
		void GenerateHiZ(ID3D12GraphicsCommandList* cmdList);

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

		Scene* mScene;

		std::unique_ptr<ColorBuffer> mFrameMap;
		std::unique_ptr<ColorBuffer> mDepthStencilMap;
		std::unique_ptr<ColorBuffer> mHiZMap;

		std::unique_ptr<StructuredBuffer> mOitppllBuffer;
		std::unique_ptr<RawBuffer> mStartOffsetBuffer; 
		std::unique_ptr<RawBuffer> mCounterBuffer;

		std::vector<std::unique_ptr<StructuredBuffer>> mCulledCommandBuffer;
        std::unique_ptr<StructuredBufferPool> mCulledCommandBufferPool;

        std::vector<std::vector<uint32_t>> mCullIdx;
		std::vector<uint32_t> mHiZIdx;
		uint32_t mHiZMipLevels;
		
		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilFormat;
		DXGI_FORMAT mHiZFormat;
	};
}