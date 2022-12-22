#include <render/global_resources.h>
#include <render/mesh.h>

void Carol::GlobalResources::DrawMeshes(ID3D12PipelineState* pso)
{
	CommandList->SetPipelineState(pso);

	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->Draw();
	}

	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->Draw();
	}

	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->Draw();
	}

	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO)
{
	CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->Draw();
	}

	CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO)
{
	CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->Draw();
	}

	CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO)
{
	CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->Draw();
	}
}

void Carol::GlobalResources::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	CommandList->SetPipelineState(skyBoxPSO);
	SkyBoxMesh->Draw();
}

