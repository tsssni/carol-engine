#pragma once
#include <render_pass/render_pass.h>
#include <scene/model.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <assimp/aabb.h>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class GlobalResources;
	class StructuredBuffer;
	class RawBuffer;

#pragma pack(1)
	class IndirectCommand
	{
	public:
		D3D12_GPU_VIRTUAL_ADDRESS MeshCBAddr;
		D3D12_GPU_VIRTUAL_ADDRESS SkinnedCBAddr;
		D3D12_DISPATCH_MESH_ARGUMENTS DispatchMeshArgs;
	};
#pragma pack()

	class MeshesPass : public RenderPass
	{

	public:
		MeshesPass(GlobalResources* globalResources);

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;
		
		void ClearCullMark();

		ID3D12CommandSignature* GetCommandSignature();
		uint32_t GetMeshCBStartOffet(MeshType type);

		uint32_t GetMeshCBIdx();
		uint32_t GetCommandBufferIdx();
		uint32_t GetInstanceFrustumCulledMarkBufferIdx();
		uint32_t GetInstanceOcclusionPassedMarkBufferIdx();

		void ExecuteIndirect(StructuredBuffer* indirectCmdBuffer);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;
		void InitCommandSignature();

		void TestBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements);
		void ResizeBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize, bool isConstant);

		Microsoft::WRL::ComPtr<ID3D12CommandSignature> mCommandSignature;

		std::vector<std::unique_ptr<StructuredBuffer>> mIndirectCommandBuffer;
		std::vector<std::unique_ptr<StructuredBuffer>> mMeshCB;
		std::vector<std::unique_ptr<StructuredBuffer>> mSkinnedCB;

		std::unique_ptr<RawBuffer> mInstanceFrustumCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mInstanceOcclusionPassedMarkBuffer;

		uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
	};
}
