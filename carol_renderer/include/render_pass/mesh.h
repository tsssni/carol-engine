#pragma once
#include <render_pass/render_pass.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <assimp/aabb.h>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class GlobalResources;
	class CircularHeap;
	class RenderNode;

	class MeshesPass : public RenderPass
	{

	public:
		MeshesPass(GlobalResources* globalResources);

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;
	
		void DrawMeshes(const std::vector<ID3D12PipelineState*>& pso, bool color = false);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;
		
		void Draw(RenderNode* renderNode, bool color);

		enum
		{
			OPAQUE_STATIC, OPAQUE_SKINNED, TRANSPARENT_STATIC, TRANSPARENT_SKINNED, MESH_TYPE_COUNT
		};

		static std::unique_ptr<CircularHeap> MeshCBHeap;
		static std::unique_ptr<CircularHeap> SkinnedCBHeap;
	};
}
