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
	class Mesh;
	class GlobalResources;
	class StructuredBuffer;
	class RawBuffer;

	class IndirectCommand
	{
	public:
		D3D12_GPU_VIRTUAL_ADDRESS MeshCB;
		uint32_t MeshConstants[6];
		D3D12_GPU_VIRTUAL_ADDRESS SkinnedCB;
		D3D12_DISPATCH_MESH_ARGUMENTS DispatchMeshArgs;
	};

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
		uint32_t GetCullMarkIdx(MeshType type);
		uint32_t GetCommandBufferIdx(MeshType type);
		void DrawMeshes(const std::vector<ID3D12PipelineState*>& pso);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;
		void InitCommandSignature();

		void Draw(Mesh* renderNode);

		Microsoft::WRL::ComPtr<ID3D12CommandSignature> mCommandSignature;
		std::vector<std::vector<IndirectCommand>> mIndirectCommand;

		std::vector<std::unique_ptr<StructuredBuffer>> mIndirectCommandBuffer;
		std::vector<std::unique_ptr<RawBuffer>> mCullMarkBuffer;
	};
}
