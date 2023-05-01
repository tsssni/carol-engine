#pragma once
#include <render_pass/render_pass.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <string_view>

namespace Carol
{
	class ColorBuffer;
	class StructuredBuffer;
	class StructuredBufferPool;
	class FastConstantBufferAllocator;
	class Scene;
	class MeshPSO;
	class ComputePSO;

	class CullConstants
	{
	public:
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 HistViewProj;
		DirectX::XMFLOAT3 EyePos;
		float CullPad0;

		uint32_t CulledCommandBufferIdx;
		uint32_t MeshCount;
		uint32_t MeshOffset;
		uint32_t HiZMapIdx;
	};

	class HiZConstants
	{
	public:
		uint32_t DepthMapIdx;
		uint32_t RWHiZMapIdx;
		uint32_t HiZMapIdx;
		uint32_t SrcMipLevel;
		uint32_t NumMipLevel;
	};

	class CullPass : public RenderPass
	{
	public:
		CullPass(
			ID3D12Device* device,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager,
			Scene* scene,
			uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT depthFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
		void Update(DirectX::XMMATRIX viewProj, DirectX::XMMATRIX histViewProj, DirectX::XMVECTOR eyePos, uint64_t cpuFenceValue, uint64_t completedFenceValue);
		
		StructuredBuffer* GetIndirectCommandBuffer(MeshType type);
		void SetDepthMap(ColorBuffer* depthMap);

	protected:
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		void CullReset(ID3D12GraphicsCommandList* cmdList);

        void CullInstances(ID3D12GraphicsCommandList* cmdList);
		void OcclusionCullInstancesRecheck(ID3D12GraphicsCommandList* cmdList);

        void CullMeshes(ID3D12GraphicsCommandList* cmdList);
        void OcclusionCullMeshesRecheck(ID3D12GraphicsCommandList* cmdList);

		void GenerateHiZ(ID3D12GraphicsCommandList* cmdList);

		Scene* mScene;
		ColorBuffer* mDepthMap;

		std::vector<std::unique_ptr<CullConstants>> mCullConstants;
		std::vector<D3D12_GPU_VIRTUAL_ADDRESS> mCullCBAddr;
		std::unique_ptr<FastConstantBufferAllocator> mCullCBAllocator;

		std::unique_ptr<HiZConstants> mHiZConstants;
		D3D12_GPU_VIRTUAL_ADDRESS mHiZCBAddr;
		std::unique_ptr<FastConstantBufferAllocator> mHiZCBAllocator;

		std::vector<std::unique_ptr<StructuredBuffer>> mCulledCommandBuffer;
        std::unique_ptr<StructuredBufferPool> mCulledCommandBufferPool;

		std::unique_ptr<ColorBuffer> mHiZMap;

		DXGI_FORMAT mDepthFormat;
		DXGI_FORMAT mHiZFormat;

		float mDepthBias;
		float mDepthBiasClamp;
		float mSlopeScaledDepthBias;
		
		std::unique_ptr<MeshPSO> mOpaqueStaticCullMeshPSO;
		std::unique_ptr<MeshPSO> mOpaqueSkinnedCullMeshPSO;
		std::unique_ptr<MeshPSO> mTransparentStaticCullMeshPSO;
		std::unique_ptr<MeshPSO> mTransparentSkinnedCullMeshPSO;
		std::unique_ptr<MeshPSO> mOpaqueHistHiZStaticCullMeshPSO;
		std::unique_ptr<MeshPSO> mOpaqueHistHiZSkinnedCullMeshPSO;
		std::unique_ptr<MeshPSO> mTransparentHistHiZStaticCullMeshPSO;
		std::unique_ptr<MeshPSO> mTransparentHistHiZSkinnedCullMeshPSO;

		std::unique_ptr<ComputePSO> mCullInstanceComputePSO;
		std::unique_ptr<ComputePSO> mHistHiZCullInstanceComputePSO;
		std::unique_ptr<ComputePSO> mHiZGenerateComputePSO;
	};
}

