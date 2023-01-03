#include <render_pass/mesh.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <scene/scene.h>
#include <scene/model.h>
#include <scene/camera.h>
#include <utils/common.h>
#include <cmath>

#define AS_GROUP_SIZE 32

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
			for (auto& meshMapPair : mGlobalResources->Scene->GetMeshes(i))
			{
				auto& mesh = meshMapPair.second;
				Draw(mesh);
			}
		}
	}
}

void Carol::MeshesPass::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	mGlobalResources->CommandList->SetPipelineState(skyBoxPSO);
	Draw(mGlobalResources->Scene->GetSkyBox());
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

void Carol::MeshesPass::Draw(Mesh* mesh)
{
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::MESH_CB, mesh->GetMeshCBGPUVirtualAddress());
	mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(RootSignature::MESH_CONSTANTS, Mesh::MESH_IDX_COUNT, mesh->GetMeshIdxData(), 0);

	if (mesh->IsSkinned())
	{
		mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::SKINNED_CB, mesh->GetSkinnedCBGPUVirtualAddress());
	}

	mGlobalResources->CommandList->DispatchMesh(ceilf(mesh->GetMeshletSize() * 1.f / AS_GROUP_SIZE), 1, 1);
}
