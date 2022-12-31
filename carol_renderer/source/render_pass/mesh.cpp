#include <render_pass/mesh.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <scene/scene.h>
#include <scene/model.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <utils/common.h>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
	
}

Carol::MeshesPass::MeshesPass(GlobalResources* globalResources)
	:RenderPass(globalResources)
{
}

void Carol::MeshesPass::Draw()
{
}

void Carol::MeshesPass::Update()
{
}

void Carol::MeshesPass::OnResize()
{
}

void Carol::MeshesPass::ReleaseIntermediateBuffers()
{
}

void Carol::MeshesPass::DrawMeshes(const std::vector<ID3D12PipelineState*>& pso)
{
	for (int i = 0; i < Scene::MESH_TYPE_COUNT; ++i)
	{
		if (pso[i])
		{
			mGlobalResources->CommandList->SetPipelineState(pso[i]);
			for (auto& renderNode : mGlobalResources->Scene->GetMeshes(i))
			{
				Draw(&renderNode);
			}
		}
	}
}

void Carol::MeshesPass::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	mGlobalResources->CommandList->SetPipelineState(skyBoxPSO);
	Draw(GetRvaluePtr(mGlobalResources->Scene->GetSkyBox()));
}

void Carol::MeshesPass::InitShaders()
{
}

void Carol::MeshesPass::InitPSOs()
{
}

void Carol::MeshesPass::InitResources()
{
}

void Carol::MeshesPass::InitDescriptors()
{
}

void Carol::MeshesPass::Draw(RenderNode* renderNode)
{
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::WORLD_CB, renderNode->WorldGPUVirtualAddress);
	mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(RootSignature::MESH_CONSTANTS, Mesh::TEX_IDX_COUNT, renderNode->Mesh->GetTexIdx().data(), 0);

	if (renderNode->Mesh->IsSkinned())
	{
		mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::SKINNED_CB, renderNode->Mesh->GetSkinnedCBGPUVirtualAddress());
	}

	mGlobalResources->CommandList->DispatchMesh(renderNode->Mesh->GetMeshletSize(), 1, 1);
}
